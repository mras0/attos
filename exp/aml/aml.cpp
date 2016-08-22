#include <attos/out_stream.h>
#include <attos/cpu.h>
#include <attos/containers.h>
#include <attos/array_view.h>
#include <attos/string.h>

namespace attos { namespace aml {

enum class opcode : uint16_t {
    zero                = 0x00,
    one                 = 0x01,
    name                = 0x08,
    byte                = 0x0a, // BytedData
    word                = 0x0b, // WordData
    dword               = 0x0c, // DWordData
    string_             = 0x0d,
    qword               = 0x0e, // QWordData
    scope               = 0x10,
    buffer              = 0x11,
    package             = 0x12,
    method              = 0x14,
    ext_prefix          = 0x5b, // '[' ByteData follows specifying the extended opcode
    store               = 0x70,
    add                 = 0x72,
    decrement           = 0x76,
    shift_left          = 0x79,
    and_                = 0x7b,
    or_                 = 0x7d,
    find_set_right_bit  = 0x82,
    deref_of            = 0x83,
    size_of             = 0x87,
    index               = 0x88,
    create_word_field   = 0x8b,
    not_                = 0x92,
    lequal              = 0x93,
    lgreater            = 0x94,
    lless               = 0x95,
    if_                 = 0xa0,
    else_               = 0xa1,
    while_              = 0xa2,
    return_             = 0xa4,
    ones                = 0xff,

    // Extended opcodes
    cond_ref_of         = 0x5b12,
    op_region           = 0x5b80,
    field               = 0x5b81,
    device              = 0x5b82,
};

constexpr bool is_extended(opcode op) {
    return static_cast<unsigned>(op) > 0xff;
}

out_stream& operator<<(out_stream& os, opcode op) {
    return os << as_hex(static_cast<unsigned>(op)).width(is_extended(op) ? 4 : 2);
}

enum class node_kind {
    normal,
    method,
};

class __declspec(novtable) node {
public:
    virtual ~node() = 0 {}

    friend out_stream& operator<<(out_stream& os, const node& n) {
        n.do_print(os);
        return os;
    }

    node_kind kind() const {
        return do_kind();
    }

private:
    virtual void do_print(out_stream& os) const = 0;
    virtual node_kind do_kind() const {
        return node_kind::normal;
    }
};
using node_ptr = kowned_ptr<node>;
using kstring  = kvector<char>;

kstring make_kstring(const char* str) {
    return kstring{str, str+string_length(str)+1};
}

class dummy_node : public node {
public:
    explicit dummy_node(const char* name) : name_(make_kstring(name)) {
    }

private:
    kstring name_;
    virtual void do_print(out_stream& os) const override {
        os << name_.begin();
    }
};

node_ptr make_text_node(const char* name) {
    return node_ptr{knew<dummy_node>(name).release()};
}

node_ptr make_text_node(const kstring& s) {
   return make_text_node(s.begin());
}

template<typename T>
class const_node : public node {
public:
    explicit const_node(T value) : value_(value) {
    }
private:
    T value_;
    virtual void do_print(out_stream& os) const override {
        os << "0x" << as_hex(value_);
    }
};

class buffer_node : public node {
public:
    explicit buffer_node(node_ptr&& buffer_size, kvector<uint8_t>&& data) : buffer_size_(std::move(buffer_size)), data_(std::move(data)) {
        // DefBuffer := BufferOp PkgLength BufferSize ByteList
        // BufferSize := TermArg => Integer
    }

private:
    node_ptr         buffer_size_;
    kvector<uint8_t> data_;
    virtual void do_print(out_stream& os) const override {
        os << "Buffer(Size=" << *buffer_size_ << ", Data[" << data_.size() << "])";
    }
};

int indent = 0;
void do_newline(out_stream& os) {
    os << "\n";
    write_many(os, ' ', indent*2);
}

template<const char* const Name>
class container_node_impl : public node {
public:
    explicit container_node_impl(kvector<node_ptr>&& elements) : elements_(std::move(elements)) {
    }

private:
    kvector<node_ptr> elements_;
    virtual void do_print(out_stream& os) const override {
        os << Name << " [";
        ++indent;
        for (const auto& p : elements_) {
            do_newline(os);
            os << *p;
        }
        --indent;
        do_newline(os);
        os << "]";
    }
};

extern const char package_node_tag[] = "Package";
using package_node = container_node_impl<package_node_tag>;

template<typename T>
node_ptr make_const_node(T value) {
    return node_ptr{knew<const_node<T>>(value).release()};
}

class name_space;

class parse_state {
public:
    parse_state(name_space& ns, array_view<uint8_t> data) : ns_(&ns), data_(data) {
    }
    ~parse_state() {
        REQUIRE(ns_);
    }

    const uint8_t* begin() const { return data_.begin(); }
    const uint8_t* end() const { return data_.end(); }
    size_t size() const { return data_.size(); }

    char peek() const {
        return *data_.begin();
    }

    opcode peek_opcode() const {
        const uint8_t first_byte = peek();
        if (static_cast<opcode>(first_byte) == opcode::ext_prefix) {
            return static_cast<opcode>((first_byte << 8) | data_[1]);
        }
        return static_cast<opcode>(first_byte);
    }

    array_view<uint8_t> consume(uint32_t size) {
        REQUIRE(data_.size() >= size);
        array_view<uint8_t> res{data_.begin(), data_.begin() + size};
        data_ = array_view<uint8_t>(data_.begin() + size, data_.end());
        return res;
    }

    uint8_t consume() {
        return consume(1)[0];
    }

    opcode consume_opcode() {
        const auto op = peek_opcode();
        consume();
        if (is_extended(op)) {
            consume();
        }
        return op;
    }

    parse_state moved_to(const uint8_t* new_begin) const {
        return parse_state(*ns_, new_begin, end());
    }

    friend parse_state adjust_with_pkg_length(parse_state data);

    name_space& ns() {
        REQUIRE(ns_);
        return *ns_;
    }

private:
    name_space*         ns_;
    array_view<uint8_t> data_;

    parse_state(name_space& ns, const uint8_t* begin, const uint8_t* end) : ns_(&ns), data_(begin, end) {
    }
};

constexpr char root_char             = '\\';
constexpr char parent_prefix_char    = '^';

class name_space {
public:
    explicit name_space() {
    }

    name_space(const name_space&) = delete;
    name_space& operator=(const name_space&) = delete;

    ~name_space() {
        REQUIRE(cur_namespace_.empty());
    }

    class scope_registration {
    public:
        void provide(node& node) {
            REQUIRE(!node_);
            node_ = &node;
        }
        ~scope_registration() {
            REQUIRE(node_);
            ns_.close_scope(name_.begin(), *node_);
        }
    private:
        friend name_space;
        name_space& ns_;
        kstring     name_;
        node*       node_ = nullptr;
        explicit scope_registration(name_space& ns, const char* name) : ns_(ns), name_(make_kstring(name)) {}
    };

    scope_registration open_scope(const char* name) {
        //dbgout() << "open_scope " << name << "\n";
        cur_namespace_.insert(cur_namespace_.end(), name, name + string_length(name));
        return scope_registration{*this, name};
    }

    node* lookup(const char* name) {
        if (name[0] == root_char) {
            if (auto* b = find_binding(name)) {
                return b->n;
            }
        } else {
            // Hack... this isn't how lookup works...
            auto ns = cur_namespace_;
            for (; *name == parent_prefix_char; ++name) {
                REQUIRE(parent_scope(ns));
            }
            REQUIRE(name[0]);
            for (;;) {
                const auto rn = relative(ns, name);
                dbgout() << "Trying " << rn.begin() << "\n";
                if (auto* b = find_binding(rn.begin())) {
                    return b->n;
                }
                if (!parent_scope(ns)) {
                    break;
                }
            }
        }
        dbgout() << "Lookup of " << name << " failed in " << hack_cur_name() << "\n";
        return nullptr;
    }

private:
    struct binding {
        kstring name;
        node*   n;
    };

    kstring          cur_namespace_; // Not NUL-terimnated!
    kvector<binding> bindings_;

    const binding* find_binding(const char* name) const {
        for (const auto& b : bindings_) {
            if (string_equal(name, b.name.begin())) {
                return &b;
            }
        }
        return nullptr;
    }

    static bool parent_scope(kstring& ns) { // ns should not be nul-terminated
        if (ns.size() < 4) {
            return false;
        }
        ns.pop_back();
        ns.pop_back();
        ns.pop_back();
        ns.pop_back();
        return true;
    }

    static kstring relative(const kstring& ns, const char* path) { // ns should not be nul-terminated
        kstring name{ns};
        name.insert(name.end(), path, path + string_length(path) + 1);
        return name;
    }

    detail::formatted_string hack_cur_name() const {
        return format_str((const char*)cur_namespace_.begin()).max_width((int)cur_namespace_.size());
    }

    void close_scope(const char* scope_name, node& n) {
        auto name = relative(cur_namespace_, "");
        dbgout() << "Registered " << name.begin() << " as " << as_hex(&n) << "\n";
        bindings_.push_back(binding{std::move(name), &n});
        REQUIRE(cur_namespace_.size() >= string_length(scope_name));
        cur_namespace_.resize(cur_namespace_.size()-string_length(scope_name));
    }
};
using scope_reg = name_space::scope_registration;

template<typename T>
struct parse_result {
    parse_state data;
    T           result;
};

template<typename Node>
parse_result<node_ptr> make_parse_result(parse_state data, kowned_ptr<Node>&& node) {
    return parse_result<node_ptr>{data, node_ptr{node.release()}};
}

auto parse_pkg_length(parse_state data)
{
    const uint8_t b0 = data.consume();
    // bit 7-6 bytes that follow (0-3)
    // bit 5-4 only used if PkgLength < 63
    // bit 3-0 least significant packet length nibble
    uint32_t len = 0;
    switch (b0 >> 6) {
        case 0:
            len = b0;
            break;
        case 1:
            len = b0 & 0xf;
            len |= (data.consume() << 4);
            break;
        case 2:
            len = b0 & 0xf;
            len |= (data.consume() << 4);
            len |= (data.consume() << 12);
            break;
        default:
            dbgout()<<"TODO: Handle " << (b0>>6) << " extra PkgLength bytes\n";
            REQUIRE(false);
    }
    return parse_result<uint32_t>{data, len};
}

parse_state adjust_with_pkg_length(parse_state data)
{
    const uint8_t pkg_length_bytes = 1+(data.peek()>>6);
    auto pkg_len = parse_pkg_length(data);
    REQUIRE(pkg_len.result >= pkg_length_bytes);
    return parse_state(*data.ns_, pkg_len.data.begin(), pkg_len.data.begin() + pkg_len.result-pkg_length_bytes);
}

constexpr bool is_lead_name_char(char c) { return c == '_' || (c >= 'A' && c <= 'Z') ;}
constexpr bool is_name_char(char c) { return is_lead_name_char(c) || (c >= '0' && c <= '9') ;}

constexpr bool valid_nameseg(const char* d) {
    return is_lead_name_char(d[0]) && is_name_char(d[1]) && is_name_char(d[2]) && is_name_char(d[3]);
}
constexpr bool valid_nameseg(const uint8_t* d) {
    return valid_nameseg(reinterpret_cast<const char*>(d));
}

constexpr char dual_name_path_start  = 0x2e;
constexpr char multi_name_path_start = 0x2f;

constexpr bool is_name_string_start(char c) {
    return c == root_char || c == parent_prefix_char || c == dual_name_path_start || c == multi_name_path_start || is_lead_name_char(c);
}

constexpr char local_0               = 0x60;
constexpr char local_7               = 0x67;
constexpr char arg_0                 = 0x68;
constexpr char arg_6                 = 0x6e;

constexpr bool is_arg_or_local(char c) {
    return c >= local_0 && c <= arg_6;
}

auto parse_name_string(parse_state data)
{
    // NameString := <RootChar NamePath> | <PrefixPath NamePath>

    kstring name;
    if (data.peek() == root_char) {
        name.push_back(data.consume());
    } else {
        while (data.peek() == parent_prefix_char) {
            name.push_back(data.consume());
        }
    }

    // NamePath := NameSeg | DualNamePath | MultiNamePath | NullName
    const auto type = data.consume();
    if (type == 0) { // NullName
        // nothing to do
    } else if (type == dual_name_path_start) {
        // 0x2E -> DualNamePath
        REQUIRE(valid_nameseg(data.begin()));
        REQUIRE(valid_nameseg(data.begin()+4));
        name.insert(name.end(), data.begin(), data.begin() + 8);
        data.consume(8);
    } else if (type == multi_name_path_start) {
        // MultiNamePath := MultiNamePrefix SegCount NameSeg(SegCount)
        // SegCount := ByteData
        const uint8_t seg_count = data.consume();
        REQUIRE(data.size() >= seg_count*4);
        for (int i = 0; i < seg_count; ++i) {
            REQUIRE(valid_nameseg(data.begin()));
            name.insert(name.end(), data.begin(), data.begin() + 4);
            data.consume(4);
        }
    } else if (is_lead_name_char(type)) {
        name.push_back(type);
        name.push_back(data.consume());
        name.push_back(data.consume());
        name.push_back(data.consume());
        REQUIRE(valid_nameseg(name.end()-4));
    } else {
        dbgout() << "type = 0x" << as_hex(type) << "\n";
        REQUIRE(false);
    }
    name.push_back('\0');
    return parse_result<kstring>{data, std::move(name)};
}

parse_result<kstring> parse_arg_or_local(parse_state data)
{
    const auto c = data.consume();
    if (c >= local_0 && c <= local_7) { // LocalObj
        char buffer[] = "Local0";
        buffer[sizeof(buffer)-2] += c-local_0;
        return parse_result<kstring>{data, make_kstring(buffer)};
    } else if (c >= arg_0 && c <= arg_6) { // ArgObj
        char buffer[] = "Arg0";
        buffer[sizeof(buffer)-2] += c-arg_0;
        return parse_result<kstring>{data, make_kstring(buffer)};
    } else {
        REQUIRE(false);
    }
}

parse_result<kstring> parse_super_name(parse_state data)
{
    // SimpleName := NameString | ArgObj | LocalObj
    // SuperName := SimpleName | DebugObj | Type6Opcode
    const uint8_t first = data.peek();
    if (is_arg_or_local(first)) {
        return parse_arg_or_local(data);
    }
    if (!is_name_string_start(first)) {
        dbgout() << "First = " << as_hex(first) << "\n";
        hexdump(dbgout(), data.begin()-10, std::min(32ULL, data.size()+10));
        REQUIRE(false);
    }
    return parse_name_string(data);
}

extern const char term_list_node_tag[] = "TermList";
using term_list_node = container_node_impl<term_list_node_tag>;
using term_list_node_ptr = kowned_ptr<term_list_node>;

parse_result<term_list_node_ptr> parse_term_list(parse_state data);
parse_result<node_ptr> parse_data_ref_object(parse_state data);
parse_result<node_ptr> parse_term_arg(parse_state data);

constexpr bool is_data_object(opcode op) {
    return op == opcode::zero
        || op == opcode::one
        || op == opcode::ones
        || op == opcode::byte
        || op == opcode::word
        || op == opcode::dword
        || op == opcode::string_
        || op == opcode::qword
        || op == opcode::buffer
        || op == opcode::package;
}

parse_result<node_ptr> parse_data_object(parse_state data)
{
    // DataObject := ComputationalData | DefPackage | DefVarPackage
    // ComputationalData := ByteConst | WordConst | DWordConst | QWordConst | String | ConstObj | RevisionOp | DefBuffer
    // ConstObj := ZeroOp | OneOp | OnesOp
    const auto op = data.consume_opcode();
    REQUIRE(is_data_object(op));
    switch (op) {
        // ConstObj
        case opcode::zero:
            return make_parse_result(data, make_const_node<uint8_t>(0));
        case opcode::one:
            return make_parse_result(data, make_const_node<uint8_t>(1));
        case opcode::ones:
            return make_parse_result(data, make_const_node<uint8_t>(0xff));
        // ByteConst
        case opcode::byte:
            {
                uint8_t b = data.consume();
                return make_parse_result(data, make_const_node(b));
            }
        // WordConst
        case opcode::word:
            {
                uint16_t w = data.consume();
                w |= data.consume() << 8;
                return make_parse_result(data, make_const_node(w));
            }
        // DwordConst
        case opcode::dword:
            {
                uint32_t d = data.consume();
                d |= static_cast<uint32_t>(data.consume()) << 8;
                d |= static_cast<uint32_t>(data.consume()) << 16;
                d |= static_cast<uint32_t>(data.consume()) << 24;
                return make_parse_result(data, make_const_node(d));
            }
        case opcode::qword:
            {
                uint64_t q = data.consume();
                q |= static_cast<uint64_t>(data.consume()) << 8;
                q |= static_cast<uint64_t>(data.consume()) << 16;
                q |= static_cast<uint64_t>(data.consume()) << 24;
                q |= static_cast<uint64_t>(data.consume()) << 32;
                q |= static_cast<uint64_t>(data.consume()) << 40;
                q |= static_cast<uint64_t>(data.consume()) << 48;
                q |= static_cast<uint64_t>(data.consume()) << 56;
                return make_parse_result(data, make_const_node(q));
            }
        case opcode::string_:
            {
                const char* text = reinterpret_cast<const char*>(data.begin());
                data.consume(static_cast<uint32_t>(string_length(text) + 1));
                return make_parse_result(data, make_text_node(text));
            }
        case opcode::buffer:
            {
                // DefBuffer := BufferOp PkgLength BufferSize ByteList
                // BufferSize := TermArg => Integer
                auto pkg_data          = adjust_with_pkg_length(data);
                auto buffer_size = parse_term_arg(pkg_data);
                //dbgout() << "Buffer size " << *buffer_size.result << " data:\n";
                //hexdump(dbgout(), buffer_size.data.begin(), buffer_size.data.size());
                return make_parse_result(data.moved_to(pkg_data.end()), knew<buffer_node>(std::move(buffer_size.result), kvector<uint8_t>(buffer_size.data.begin(), buffer_size.data.end())));
            }
        case opcode::package:
            {
                //DefPackage := PackageOp PkgLength NumElements PackageElementList
                //NumElements := ByteData
                //PackageElementList := Nothing | <PackageElement PackageElementList>
                //PackageElement := DataRefObject | NameString
                auto pkg_data = adjust_with_pkg_length(data);
                const uint8_t num_elements = pkg_data.consume();
                kvector<node_ptr> elements;
                while (pkg_data.begin() != pkg_data.end()) {
                    if (is_name_string_start(pkg_data.peek())) {
                        auto name = parse_name_string(pkg_data);
                        //dbgout() << "PackageElement NameString " << name.result.begin() << "\n";
                        elements.push_back(make_text_node(name.result.begin()));
                        pkg_data = name.data;
                    } else {
                        auto data_ref_object = parse_data_ref_object(pkg_data);
                        //dbgout() << "PackageElement DataRefObject " << *data_ref_object.result << "\n";
                        elements.push_back(std::move(data_ref_object.result));
                        pkg_data = data_ref_object.data;
                    }
                }
                REQUIRE(num_elements == elements.size());
                return make_parse_result(data.moved_to(pkg_data.begin()), knew<package_node>(std::move(elements)));
            }
        default:
            hexdump(dbgout(), data.begin()-2, std::min(data.size()+2, 64ULL));
            dbgout() << "Unhandled opcode " << op << "\n";
            REQUIRE(false);
    }
}

parse_result<kstring> parse_target(parse_state data) {
    // Target := SuperName | NullName
    if (data.peek() == 0) {
        data.consume();
        return parse_result<kstring>{data, make_kstring("null_name")};
    } else {
        return parse_super_name(data);
    }
}

class op_node : public node {
public:
    explicit op_node(const char* name, node_ptr&& arg0) : name_(make_kstring(name)), arg0_(std::move(arg0)) {
    }
    explicit op_node(const char* name, node_ptr&& arg0, kstring&& target) : name_(make_kstring(name)), arg0_(std::move(arg0)), target_(make_text_node(target)) {
    }
    explicit op_node(const char* name, node_ptr&& arg0, node_ptr&& arg1) : name_(make_kstring(name)), arg0_(std::move(arg0)), arg1_(std::move(arg1)) {
    }
    explicit op_node(const char* name, node_ptr&& arg0, node_ptr&& arg1, kstring&& target) : name_(make_kstring(name)), arg0_(std::move(arg0)), arg1_(std::move(arg1)), target_(make_text_node(target)) {
    }
private:
    kstring  name_;
    node_ptr arg0_;
    node_ptr arg1_;
    node_ptr target_;

    virtual void do_print(out_stream& os) const override {
        os << name_.begin() << " " << *arg0_;
        if (arg1_) os << " " << *arg1_;
        if (target_) os << " -> " << *target_;
    }
};

class method_node : public node {
public:
    explicit method_node(scope_reg&& ns_reg, kstring&& name, uint8_t flags, term_list_node_ptr&& statements) : name_(std::move(name)), flags_(flags), statements_(std::move(statements)) {
        ns_reg.provide(*this);
    }
    /* MethodFlags := ByteData // bit 0-2: ArgCount (0-7)
     *                         // bit 3: SerializeFlag
     *                         // 0 NotSerialized
     *                         // 1 Serialized
     *                         // bit 4-7: SyncLevel (0x00-0x0f)
     */
    uint8_t arg_count() const { return flags_ & 0x07; }
    bool    serialized() const { return !!(flags_ & 0x08); }
    uint8_t sync_level() const { return flags_>>4; }

private:
    kstring            name_;
    uint8_t            flags_;
    term_list_node_ptr statements_;
    virtual void do_print(out_stream& os) const override {
        os << "Method " << name_.begin() << " Flags " << as_hex(flags_) << " " << *statements_;
    }

    virtual node_kind do_kind() const {
        return node_kind::method;
    }
};

method_node* method_cast(node& n) {
    if (n.kind() == node_kind::method) {
        return static_cast<method_node*>(&n);
    }
    return nullptr;
}


parse_result<node_ptr> parse_unary_op(parse_state data, const char* name) {
    auto arg    = parse_term_arg(data);
    auto target = parse_target(arg.data);
    //dbgout() << name << " " << *arg.result << " -> " << target.result.begin() << "\n";
    return make_parse_result(target.data, knew<op_node>(name, std::move(arg.result), std::move(target.result)));
}

parse_result<node_ptr> parse_unary_op_no_target(parse_state data, const char* name) {
    auto arg = parse_super_name(data);
    //dbgout() << name << " " << arg.result.begin() << "\n";
    return make_parse_result(arg.data, knew<op_node>(name, make_text_node(arg.result)));
}

parse_result<node_ptr> parse_binary_op(parse_state data, const char* name) {
    // DefAnd := AndOp Operand Operand Target
    // Operand := TermArg => Integer
    // Target := SuperName | NullName
    auto arg0   = parse_term_arg(data);
    auto arg1   = parse_term_arg(arg0.data);
    auto target = parse_target(arg1.data);
    //dbgout() << name << " " << *arg0.result << ", " << *arg1.result << " -> " << target.result.begin() << "\n";
    return make_parse_result(target.data, knew<op_node>(name, std::move(arg0.result), std::move(arg1.result), std::move(target.result)));
}

parse_result<node_ptr> parse_logical_op(parse_state data, const char* name) {
    auto arg0   = parse_term_arg(data);
    auto arg1   = parse_term_arg(arg0.data);
    //dbgout() << name << " " << *arg0.result << ", " << *arg1.result << "\n";
    return make_parse_result(arg1.data, knew<op_node>(name, std::move(arg0.result), std::move(arg1.result)));
}

constexpr bool is_type2_opcode(opcode op) {
    return op == opcode::decrement
        || op == opcode::add
        || op == opcode::shift_left
        || op == opcode::and_
        || op == opcode::or_
        || op == opcode::find_set_right_bit
        || op == opcode::deref_of
        || op == opcode::size_of
        || op == opcode::index
        || op == opcode::not_
        || op == opcode::lequal
        || op == opcode::lgreater
        || op == opcode::lless
        || op == opcode::cond_ref_of;
}

parse_result<node_ptr> parse_type2_opcode(parse_state data) {
    const auto op = data.consume_opcode();
    if (!is_type2_opcode(op)) {
        dbgout() << "op = " << op << "\n";
        REQUIRE(false);
    }
    switch (op) {
        case opcode::add:
            return parse_binary_op(data, "add");
        case opcode::decrement:
            // DefDecrement := DecrementOp SuperName
            return parse_unary_op_no_target(data, "decrement");
        case opcode::shift_left:
            return parse_binary_op(data, "shiftleft");
        case opcode::and_:
            return parse_binary_op(data, "and");
        case opcode::or_:
            return parse_binary_op(data, "or");
        case opcode::find_set_right_bit:
            return parse_unary_op(data, "find_set_right_bit");
        case opcode::deref_of:
            {
                auto arg = parse_term_arg(data);
                // DefDerefOf := DerefOfOp ObjReference
                // ObjReference := TermArg => ObjectReference | String
                //dbgout() << "deref_of " << *arg.result << "\n";
                return make_parse_result(arg.data, knew<op_node>("deref_of", std::move(arg.result)));
            }
        case opcode::size_of:
            return parse_unary_op_no_target(data, "sizeof");
        case opcode::index:
            {
                // DefIndex := IndexOp BuffPkgStrObj IndexValue Target
                // BuffPkgStrObj := TermArg => Buffer, Package or String
                // IndexValue := TermArg => Integer
                return parse_binary_op(data, "index");
            }
        case opcode::not_:
            {
                // XXX: Should this be handled like extended opcodes (0x5bXY)?
                const auto next = data.peek_opcode();
                if (next == opcode::lequal) {
                    data.consume();
                    return parse_logical_op(data, "lnotequal");
                } else if (next == opcode::lgreater) {
                    data.consume();
                    return parse_logical_op(data, "lnotgreater");
                } else if (next == opcode::lless) {
                    data.consume();
                    return parse_logical_op(data, "lnotless");
                }
                return parse_unary_op(data, "not");
            }
        case opcode::lequal:
            return parse_logical_op(data, "lequal");
        case opcode::lgreater:
            return parse_logical_op(data, "lgreater");
        case opcode::lless:
            return parse_logical_op(data, "lless");
        case opcode::cond_ref_of:
            {
                // DefCondRefOf := CondRefOfOp SuperName Target
                auto name   = parse_super_name(data);
                auto target = parse_target(name.data);
                //dbgout() << "CondRefOf " << name.result.begin() << " -> " << target.result.begin() << "\n";
                return make_parse_result(target.data, knew<op_node>("cond_ref_of", make_text_node(name.result), std::move(target.result)));
            }
            break;
        default:
            hexdump(dbgout(), data.begin()-2, std::min(data.size()+2, 64ULL));
            dbgout() << "Unhandled opcode " << op << "\n";
            REQUIRE(false);
    }

    REQUIRE(false);
}

parse_result<node_ptr> parse_term_arg(parse_state data) {
    // TermArg := Type2Opcode | DataObject | ArgObj | LocalObj
    const uint8_t first = data.peek();
    //dbgout() << "parse_term_arg first = " << as_hex(first) << "\n";
    if (is_arg_or_local(first)) {
        auto name = parse_arg_or_local(data);
        return make_parse_result(name.data, make_text_node(name.result));
    } else if (is_name_string_start(first)) {
        // It seems NameSeg is valid here. E.g. ReturnOp 'TMP_'
        // Is this sometimes a method invocation?
        auto name = parse_name_string(data);
        data = name.data;
        if (auto n = data.ns().lookup(name.result.begin())) {
            dbgout () << "Known name: " << *n << "\n";
            if (auto m = method_cast(*n)) {
                dbgout() << "Method with " << m->arg_count() << " argument(s)\n";
                for (int i = 0; i < m->arg_count(); ++i) {
                    auto arg = parse_term_arg(data);
                    dbgout() << "Arg" << i << ": " << *arg.result << "\n";
                    data = arg.data;
                }
            }
        }
        return make_parse_result(data, make_text_node(name.result));
    } else if (is_data_object(static_cast<opcode>(first))) {
        return parse_data_object(data);
    } else {
        // Type2Opcode
        return parse_type2_opcode(data);
    }
}


parse_result<uint8_t> parse_field_flags(parse_state data) {
    const uint8_t flags = data.consume();
    return {data, flags};
}

class named_field_node : public node {
public:
    explicit named_field_node(kstring&& name, uint32_t len) : name_(std::move(name)), len_(len) {
    }

private:
    kstring  name_;
    uint32_t len_;

    virtual void do_print(out_stream& os) const override {
        os << "NamedField " << name_.begin() << " Length " << as_hex(len_).width(0);
    }
};
extern const char field_list_node_tag[] = "FieldList";
using field_list_node = container_node_impl<field_list_node_tag>;
using field_list_node_ptr = kowned_ptr<field_list_node>;

class field_node : public node {
public:
    explicit field_node(kstring&& name, uint8_t flags, field_list_node_ptr&& fields) : name_(std::move(name)), flags_(flags), fields_(std::move(fields)) {
    }

private:
    kstring             name_;
    uint8_t             flags_;
    field_list_node_ptr fields_;
    virtual void do_print(out_stream& os) const override {
        os << "Field " << name_.begin() << " Flags " << as_hex(flags_) << " " << *fields_;
    }
};

// DefField := FieldOp PkgLength NameString FieldFlags FieldList

parse_result<node_ptr> parse_named_field(parse_state data)
{
    REQUIRE(data.size() >= 4);
    REQUIRE(valid_nameseg(reinterpret_cast<const char*>(data.begin())));
    kstring name;
    name.push_back(data.consume());
    name.push_back(data.consume());
    name.push_back(data.consume());
    name.push_back(data.consume());
    name.push_back('\0');
    const auto len = parse_pkg_length(data);
    //dbgout() << "NamedField " << name.begin() << " PkgLen 0x" <<  as_hex(len.result).width(0) << "\n";
    return make_parse_result(len.data, knew<named_field_node>(std::move(name), len.result));
}

parse_result<field_list_node_ptr> parse_field_list(parse_state data)
{
    // FieldList := Nothing | <FieldElement FieldList>
    kvector<node_ptr> elements;
    while (data.begin() != data.end()) {
        // FieldElement  := NamedField | ReservedField | AccessField | ExtendedAccessField | ConnectField
        // NamedField    := NameSeg PkgLength
        // ReservedField := 0x00 PkgLength
        // AccessField   := 0x01 AccessType AccessAttrib
        // ConnectField  := <0x02 NameString> | <0x02 BufferData>
        // ExtendedAccessField := 0x03 AccessType ExtendedAccessAttrib AccessLength
        const uint8_t first = data.peek();
        if (first == 0) {
            data.consume();
            const auto len = parse_pkg_length(data);
            //dbgout() << "ReservedField " << " PkgLen 0x" <<  as_hex(len.result).width(0) << "\n";
            elements.push_back(make_text_node("ReservedField"));
            data = len.data;
        } else if (is_lead_name_char(first)) {
            auto named_field = parse_named_field(data);
            elements.push_back(std::move(named_field.result));
            data = named_field.data;
        } else {
            dbgout() << "Unknown FieldElement " << as_hex(first) << "\n";
            REQUIRE(false);
        }
    }
    return {data, knew<field_list_node>(std::move(elements))};
}

extern const char obj_list_node_tag[] = "ObjectList";
using obj_list_node = container_node_impl<obj_list_node_tag>;
using obj_list_node_ptr = kowned_ptr<obj_list_node>;

parse_result<obj_list_node_ptr> parse_object_list(parse_state data);

class device_node : public node {
public:
    explicit device_node(scope_reg&& ns_reg, kstring&& name, obj_list_node_ptr&& objs) : name_(std::move(name)), objs_(std::move(objs)) {
        ns_reg.provide(*this);
    }

private:
    kstring            name_;
    obj_list_node_ptr objs_;
    virtual void do_print(out_stream& os) const override {
        os << "Device " << name_.begin() << " " << *objs_;
    }
};

class op_region_node : public node {
public:
    explicit op_region_node(kstring&& name, uint8_t space, node_ptr&& offset, node_ptr&& len)
        : name_(std::move(name))
        , space_(space)
        , offset_(std::move(offset))
        , len_(std::move(len)) {
    }

private:
    kstring  name_;
    uint8_t  space_;
    node_ptr offset_;
    node_ptr len_;
    virtual void do_print(out_stream& os) const override {
        os << "DefOpRegion " << name_.begin() << " Space " << as_hex(space_) << " Offset " << *offset_ << " Len " << *len_;
    }
};


parse_result<node_ptr> parse_data_ref_object(parse_state data)
{
    // DataRefObject := DataObject | ObjectReference | DDBHandle
    return parse_data_object(data); // TODO: ObjectReference | DDBHandle
}

class name_node : public node {
public:
    explicit name_node(scope_reg&& ns_reg, kstring&& name, node_ptr&& obj) : name_(std::move(name)), obj_(std::move(obj)) {
        ns_reg.provide(*this);
    }

    const char* name() const {
        return name_.begin();
    }

private:
    kstring     name_;
    node_ptr    obj_;
    virtual void do_print(out_stream& os) const override {
        os << "Name " << name() << " " << *obj_;
    }
};

class scope_node : public node {
public:
    explicit scope_node(scope_reg&& ns_reg, kstring&& name, term_list_node_ptr&& statements) : name_(std::move(name)), statements_(std::move(statements)) {
        ns_reg.provide(*this);
    }

private:
    kstring            name_;
    term_list_node_ptr statements_;
    virtual void do_print(out_stream& os) const override {
        os << "Scope " << name_.begin() << " " << *statements_;
    }
};

class if_node : public node {
public:
    explicit if_node(node_ptr&& predicate, term_list_node_ptr&& if_statements, term_list_node_ptr&& else_statements)
        : predicate_(std::move(predicate))
        , if_statements_(std::move(if_statements))
        , else_statements_(std::move(else_statements)) {
    }

private:
    node_ptr           predicate_;
    term_list_node_ptr if_statements_;
    term_list_node_ptr else_statements_;
    virtual void do_print(out_stream& os) const override {
        os << "If (" << *predicate_ << ") { " << *if_statements_ << "}";
        if (else_statements_) {
            os << " Else { " << *else_statements_ << "}";
        }
    }
};

parse_result<node_ptr> parse_term_obj(parse_state data)
{
    // TermObj := NameSpaceModifierObject | NamedObj | Type1Opcode | Type2Opcode
    // NameSpaceModifierObject := DefAlias | DefName | DefScope
    const auto op = data.peek_opcode();
    if (is_type2_opcode(op)) {
        return parse_type2_opcode(data);
    }
    data.consume_opcode();
    switch (op) {
        case opcode::name:
            {
                // DefName := NameOp NameString DataRefOjbect
                auto name            = parse_name_string(data);
                auto scope_reg       = data.ns().open_scope(name.result.begin());
                auto data_ref_object = parse_data_ref_object(name.data);
                //dbgout() << "DefName " << name.result.begin() << " " << *data_ref_object.result << "\n";
                return make_parse_result(data_ref_object.data, knew<name_node>(std::move(scope_reg), std::move(name.result), std::move(data_ref_object.result)));
            }
        case opcode::scope:
            {
                // DefScope := ScopeOp PkgLength NameString TermList
                auto name      = parse_name_string(adjust_with_pkg_length(data));
                auto scope_reg = data.ns().open_scope(name.result.begin());
                auto term_list = parse_term_list(name.data);
                //dbgout() << "DefScope " << name.result.begin() << "\n";
                return make_parse_result(data.moved_to(term_list.data.begin()), knew<scope_node>(std::move(scope_reg), std::move(name.result), std::move(term_list.result)));
            }
        case opcode::method:
            {
                // DefMethod := MethodOp PkgLength NameString MethodFlags TermList
                auto pkg_data = adjust_with_pkg_length(data);
                auto name = parse_name_string(pkg_data);
                auto scope_reg = data.ns().open_scope(name.result.begin());
                pkg_data = name.data;
                uint8_t method_flags = pkg_data.consume();
                auto term_list = parse_term_list(pkg_data);
                //dbgout() << "DefMethod " << name.result.begin() << " Flags " << as_hex(method_flags) << "\n";
                return make_parse_result(data.moved_to(term_list.data.begin()), knew<method_node>(std::move(scope_reg), std::move(name.result), method_flags, std::move(term_list.result)));
            }
        case opcode::store:
            {
                // DefStore := StoreOp TermArg SuperName
                auto arg  = parse_term_arg(data);
                auto name = parse_super_name(arg.data);
                //dbgout() << "Store " << *arg.result << " -> " << name.result.begin() << "\n";
                return make_parse_result(name.data, knew<op_node>("store", std::move(arg.result), std::move(name.result)));
            }
        case opcode::create_word_field:
            {
                // DefCreateWordField := CreateWordFieldOp SourceBuff ByteIndex NameString
                // SourceBuff := TermArg => Buffer
                // ByteIndex := TermArg => Integer
                auto source_buffer = parse_term_arg(data);
                auto byte_index    = parse_term_arg(source_buffer.data);
                auto name          = parse_name_string(byte_index.data);
                //dbgout() << "DefCreateWordField " << *source_buffer.result << " Index " << *byte_index.result << " Name " << name.result.begin() << "\n";
                return make_parse_result(name.data, knew<op_node>("CreateWordField", std::move(source_buffer.result), std::move(byte_index.result), std::move(name.result)));
            }
            break;
        case opcode::if_:
            {
                // DefIfElse := IfOp PkgLength Predicate TermList DefElse
                // Predicate := TermArg => Integer
                auto predicate = parse_term_arg(adjust_with_pkg_length(data));
                //dbgout() << "DefIfElse predicate = " << *predicate.result << "\n";
                auto term_list = parse_term_list(predicate.data);
                data = data.moved_to(term_list.data.begin());
                // DefElse := Nothing | <ElseOp PkgLength TermList>
                term_list_node_ptr else_statements;
                if (data.peek_opcode() == opcode::else_) {
                    data.consume_opcode();
                    auto else_term_list = parse_term_list(adjust_with_pkg_length(data));
                    data = data.moved_to(else_term_list.data.begin());
                    else_statements = std::move(else_term_list.result);
                }
                return make_parse_result(data, knew<if_node>(std::move(predicate.result), std::move(term_list.result), std::move(else_statements)));
            }
        case opcode::while_:
            {
                // DefWhile := WhileOp PkgLength Predicate TermList
                const auto predicate = parse_term_arg(adjust_with_pkg_length(data));
                dbgout() << "DefWhile predicate = " << *predicate.result << "\n";
                const auto term_list = parse_term_list(predicate.data);
                return make_parse_result(data.moved_to(term_list.data.begin()), knew<dummy_node>("while"));
            }
        case opcode::return_:
            {
                // DefReturn := ReturnOp ArgObject
                // ArgObject := TermArg => DataRefObject
                auto arg = parse_term_arg(data);
                //dbgout() << "Return " << *arg.result << "\n";
                return make_parse_result(arg.data, knew<op_node>("return", std::move(arg.result)));
            }
        case opcode::op_region:
            {
                // DefOpRegion := OpRegionOp NameString RegionSpace(byte) RegionOffset(term=>int) RegionLen(term=>int)
                auto name = parse_name_string(data);
                data = name.data;
                uint8_t region_space = data.consume();
                auto region_offset   = parse_term_arg(data);
                auto region_len      = parse_term_arg(region_offset.data);
                //dbgout() << "DefOpRegion " << name.result.begin() << " Region space " << as_hex(region_space) << " offset " << *region_offset.result << " len " << *region_len.result << "\n";
                return make_parse_result(region_len.data, knew<op_region_node>(std::move(name.result), region_space, std::move(region_offset.result), std::move(region_len.result)));
            }
        case opcode::field:
            {
                // DefField := FieldOp PkgLength NameString FieldFlags FieldList
                auto name        = parse_name_string(adjust_with_pkg_length(data));
                auto field_flags = parse_field_flags(name.data);
                //dbgout() << "DefField " << name.result.begin() << " flags " << *field_flags.result << "\n";
                auto field_list  = parse_field_list(field_flags.data);
                return make_parse_result(data.moved_to(field_list.data.begin()), knew<field_node>(std::move(name.result), field_flags.result, std::move(field_list.result)));
            }
        case opcode::device:
            {
                // DefDevice := DeviceOp PkgLength NameString ObjectList
                auto name        = parse_name_string(adjust_with_pkg_length(data));
                auto scope_reg   = data.ns().open_scope(name.result.begin());
                auto object_list = parse_object_list(name.data);
                //dbgout() << "DefDevice " << name.result.begin() << "\n";
                //return make_parse_result(parse_state(object_list.data.begin(), data.end()), knew<dummy_node>("device"));
                return make_parse_result(data.moved_to(object_list.data.begin()), knew<device_node>(std::move(scope_reg), std::move(name.result), std::move(object_list.result)));
            }
        default:
            dbgout() << "Unhandled opcode " << op << "\n";
            hexdump(dbgout(), data.begin()-1, std::min(32ULL, data.size()));
            REQUIRE(false);
    }
}

template<typename P>
parse_result<kvector<node_ptr>> parse_list(parse_state state, P p)
{
    kvector<node_ptr> objs;
    while (state.begin() != state.end()) {
        auto res = p(state);
        state = res.data;
        objs.push_back(std::move(res.result));
    }
    return {state, std::move(objs)};
}

parse_result<obj_list_node_ptr> parse_object_list(parse_state state)
{
    // ObjectList := Nothing | <Object ObjectList>
    // Object := NameSpaceModifierObj | NamedObj
    auto objs = parse_list(state, &parse_term_obj); // TODO: Type1Opcode | Type2Opcode not allowed in ObjectList
    return {objs.data, knew<obj_list_node>(std::move(objs.result))};
}

parse_result<term_list_node_ptr> parse_term_list(parse_state state)
{
    // TermList := Nothing | <TermObj TermList>
    auto terms = parse_list(state, &parse_term_obj);
    return {terms.data, knew<term_list_node>(std::move(terms.result))};
}

void process(array_view<uint8_t> data)
{
    name_space ns;
    // HACK... See ACPI 6.1: 5.7.2 _OSI (Operating System Interfaces)
    auto osi_method = node_ptr{knew<method_node>(ns.open_scope("\\_OSI"), make_kstring("\\_OSI"), uint8_t(1), knew<term_list_node>(kvector<node_ptr>())).release()};
    parse_state state{ns, data};
#if 1
    dbgout() << *parse_term_list(state).result << "\n";
#else
    parse_term_list(state);
#endif
}

} } // namespace attos::aml

using namespace attos;

#include <fstream>
bool read_file(const char* filename, kvector<uint8_t>& data)
{
    std::ifstream in(filename, std::ifstream::binary);
    if (!in.is_open()) {
        return false;
    }
    in.seekg(0, std::ios::end);
    data.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(reinterpret_cast<char*>(&data[0]), data.size());
    return true;
}

int main(int argc, char* argv[])
{
    kvector<uint8_t> data;
    //const char* filename = "vmware_dsdt.aml";
    const char* filename = argc >= 2 ? argv[1] : "bochs_dsdt.aml";
    if (!read_file(filename, data)) {
        dbgout() << "Could not read " << filename << "\n";
        return 1;
    }
    aml::process(make_array_view(data.begin(), data.size()));
}

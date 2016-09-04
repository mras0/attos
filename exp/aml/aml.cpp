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
    string              = 0x0d,
    qword               = 0x0e, // QWordData
    scope               = 0x10,
    buffer              = 0x11,
    package             = 0x12,
    method              = 0x14,
    ext_prefix          = 0x5b, // '[' ByteData follows specifying the extended opcode
    local_0             = 0x60,
    local_1             = 0x61,
    local_2             = 0x62,
    local_3             = 0x63,
    local_4             = 0x64,
    local_5             = 0x65,
    local_6             = 0x66,
    local_7             = 0x67,
    arg_0               = 0x68,
    arg_1               = 0x69,
    arg_2               = 0x6a,
    arg_3               = 0x6b,
    arg_4               = 0x6c,
    arg_5               = 0x6d,
    arg_6               = 0x6e,
    store               = 0x70,
    add                 = 0x72,
    concat              = 0x73,
    subtract            = 0x74,
    increment           = 0x75,
    decrement           = 0x76,
    multiply            = 0x77,
    divide              = 0x78,
    shift_left          = 0x79,
    shift_right         = 0x7a,
    and_                = 0x7b,
    or_                 = 0x7d,
    xor_                = 0x7f,
    not_                = 0x80,
    find_set_left_bit   = 0x81,
    find_set_right_bit  = 0x82,
    deref_of            = 0x83,
    notify              = 0x86,
    size_of             = 0x87,
    index               = 0x88,
    create_dword_field  = 0x8a,
    create_word_field   = 0x8b,
    create_byte_field   = 0x8c,
    create_bit_field    = 0x8d,
    object_type         = 0x8e,
    create_qword_field  = 0x8f,
    land                = 0x90,
    lor                 = 0x91,
    lnot                = 0x92,
    lequal              = 0x93,
    lgreater            = 0x94,
    lless               = 0x95,
    mid                 = 0x9e,
    if_                 = 0xa0,
    else_               = 0xa1,
    while_              = 0xa2,
    noop                = 0xa3,
    return_             = 0xa4,
    ones                = 0xff,

    // Extended opcodes
    mutex_              = 0x5b01,
    cond_ref_of         = 0x5b12,
    create_field        = 0x5b13,
    sleep               = 0x5b22,
    acquire             = 0x5b23,
    release             = 0x5b27,
    fatal               = 0x5b32,
    op_region           = 0x5b80,
    field               = 0x5b81,
    device              = 0x5b82,
    processor           = 0x5b83,
    power_res           = 0x5b84,
    thermal_zone        = 0x5b85,
    index_field         = 0x5b86,

    // HACKS:
    reserved_field      = 0x8000,
    access_field,
    named_field,
    package_element,

    null_name,
    constant,
    name_lookup,
    method_call,

    dead_scope          = 0xfffe,
    invalid             = 0xffff,
};

constexpr bool is_extended(opcode op) {
    return static_cast<unsigned>(op) > 0xff;
}

out_stream& operator<<(out_stream& os, opcode op) {
    switch (op) {
        case opcode::zero:                return os << "Zero";
        case opcode::one:                 return os << "One";
        case opcode::name:                return os << "Name";
        case opcode::byte:                return os << "Byte";
        case opcode::word:                return os << "Word";
        case opcode::dword:               return os << "Dword";
        case opcode::string:              return os << "String";
        case opcode::qword:               return os << "Qword";
        case opcode::scope:               return os << "Scope";
        case opcode::buffer:              return os << "Buffer";
        case opcode::package:             return os << "Package";
        case opcode::method:              return os << "Method";
        case opcode::ext_prefix:          return os << "ExtPrefix";
        case opcode::local_0:             return os << "Local0";
        case opcode::local_1:             return os << "Local1";
        case opcode::local_2:             return os << "Local2";
        case opcode::local_3:             return os << "Local3";
        case opcode::local_4:             return os << "Local4";
        case opcode::local_5:             return os << "Local5";
        case opcode::local_6:             return os << "Local6";
        case opcode::local_7:             return os << "Local7";
        case opcode::arg_0:               return os << "Arg0";
        case opcode::arg_1:               return os << "Arg1";
        case opcode::arg_2:               return os << "Arg2";
        case opcode::arg_3:               return os << "Arg3";
        case opcode::arg_4:               return os << "Arg4";
        case opcode::arg_5:               return os << "Arg5";
        case opcode::arg_6:               return os << "Arg6";
        case opcode::store:               return os << "Store";
        case opcode::add:                 return os << "Add";
        case opcode::concat:              return os << "Concat";
        case opcode::subtract:            return os << "Subtract";
        case opcode::increment:           return os << "Increment";
        case opcode::decrement:           return os << "Decrement";
        case opcode::multiply:            return os << "Multiply";
        case opcode::divide:              return os << "Divide";
        case opcode::shift_left:          return os << "ShiftLeft";
        case opcode::shift_right:         return os << "ShiftRight";
        case opcode::and_:                return os << "And";
        case opcode::or_:                 return os << "Or";
        case opcode::xor_:                return os << "Xor";
        case opcode::not_:                return os << "Not";
        case opcode::find_set_left_bit:   return os << "FindSetLeftBit";
        case opcode::find_set_right_bit:  return os << "FindSetRightBit";
        case opcode::deref_of:            return os << "DerefOf";
        case opcode::notify:              return os << "Notify";
        case opcode::size_of:             return os << "SizeOf";
        case opcode::index:               return os << "Index";
        case opcode::create_dword_field:  return os << "DwordField";
        case opcode::create_word_field:   return os << "WordField";
        case opcode::create_byte_field:   return os << "ByteField";
        case opcode::create_bit_field:    return os << "BitField";
        case opcode::object_type:         return os << "ObjectType";
        case opcode::create_qword_field:  return os << "CreateQwordField";
        case opcode::land:                return os << "LAnd";
        case opcode::lor:                 return os << "LOr";
        case opcode::lnot:                return os << "LNot";
        case opcode::lequal:              return os << "LEqual";
        case opcode::lgreater:            return os << "LGreater";
        case opcode::lless:               return os << "LLess";
        case opcode::mid:                 return os << "Mid";
        case opcode::if_:                 return os << "If";
        case opcode::else_:               return os << "Else";
        case opcode::while_:              return os << "While";
        case opcode::noop:                return os << "Noop";
        case opcode::return_:             return os << "Return";
        case opcode::ones:                return os << "Ones";

        // Extended opcodes
        case opcode::mutex_:              return os << "Mutex";
        case opcode::cond_ref_of:         return os << "CondRefOf";
        case opcode::create_field:        return os << "CreateField";
        case opcode::sleep:               return os << "Sleep";
        case opcode::acquire:             return os << "Acquire";
        case opcode::release:             return os << "Release";
        case opcode::fatal:               return os << "Fatal";
        case opcode::op_region:           return os << "OpRegion";
        case opcode::field:               return os << "Field";
        case opcode::device:              return os << "Device";
        case opcode::processor:           return os << "Processor";
        case opcode::power_res:           return os << "PowerRes";
        case opcode::thermal_zone:        return os << "ThermalZone";
        case opcode::index_field:         return os << "IndexField";

        // HACKS:
        case opcode::reserved_field:      return os << "ReservedField";
        case opcode::access_field:        return os << "AccessField";
        case opcode::named_field:         return os << "NamedField";
        case opcode::package_element:     return os << "PackageElement";
        case opcode::null_name:           return os << "NullName";
        case opcode::constant:            return os << "Constant";
        case opcode::name_lookup:         return os << "NameLookup";
        case opcode::method_call:         return os << "MethodCall";
        case opcode::dead_scope:          return os << "<DeadScope>";
        case opcode::invalid:             return os << "<Unknown>";
    }
    dbgout() << as_hex(static_cast<unsigned>(op)).width(is_extended(op) ? 4 : 2);
    REQUIRE(false);
    return os << as_hex(static_cast<unsigned>(op)).width(is_extended(op) ? 4 : 2);
}

using kstring  = kvector<char>;

class context {
public:
    explicit context() {}
};

enum class value_type { nil, integer, string };
out_stream& operator<<(out_stream& os, value_type t) {
    return os << "value_type{" << static_cast<int>(t) << "}";
}

class value {
public:
    explicit value() : type_(value_type::nil) {
    }

    explicit value(uint64_t val, uint8_t bits) : type_(value_type::integer) {
        val_.uint.val = val;
        val_.uint.bits = bits;
    }

    explicit value(const kstring& str) : type_(value_type::string) {
        REQUIRE(str.size() <= string_max);
        REQUIRE(!str.empty() && str.back() == '\0');
        memcpy(val_.str, str.begin(), str.size());
    }

    value_type type() const {
        return type_;
    }

    auto as_uint() const {
        REQUIRE(type_ == value_type::integer);
        return val_.uint;
    }

    friend out_stream& operator<<(out_stream& os, const value& v) {
        switch (v.type_) {
            case value_type::nil:     return os << "(nil)";
            case value_type::integer: return os << "0x" << as_hex(v.val_.uint.val).width((3+v.val_.uint.bits)/4);
            case value_type::string:  return os << '"' << v.val_.str << '"';
        }
        dbgout() << __func__ << " Unhandled: " << v.type_ << "\n";
        REQUIRE(false);
    }

private:
    value_type type_;

    static constexpr int string_max = 64;
    union {
        struct {
            uint64_t val;
            uint8_t  bits;
        } uint;
        char str[string_max];
    } val_;
};

class __declspec(novtable) node {
public:
    virtual ~node() = 0 {}

    friend out_stream& operator<<(out_stream& os, const node& n) {
        n.do_print(os);
        return os;
    }

    opcode op() const {
        return do_opcode();
    }

    bool is_container() const {
        return do_is_container();
    }

    value eval(context& ctx) const {
        return do_eval(ctx);
    }

private:
    virtual void do_print(out_stream& os) const {
        os << op();
    }
    virtual opcode do_opcode() const {
        return opcode::invalid;
    }
    virtual bool do_is_container() const {
        return false;
    }
    virtual value do_eval(context& ctx) const {
        (void)ctx;
        dbgout() << "eval not implemented for " << op() << "\n";
        REQUIRE(!"Not implemented");
        return value{};
    }
};
using node_ptr = kowned_ptr<node>;

kstring make_kstring(const char* str) {
    return kstring{str, str+string_length(str)+1};
}

class simple_node : public node {
public:
    explicit simple_node(opcode op) : op_(op) {
    }

private:
    opcode op_;
    virtual opcode do_opcode() const override {
        return op_;
    }
};

node_ptr make_simple_node(opcode op) {
    return node_ptr{knew<simple_node>(op).release()};
}

class text_node : public node {
public:
    explicit text_node(opcode op, const char* name) : op_(op), name_(make_kstring(name)) {
        REQUIRE(op != opcode::name);
    }

private:
    opcode  op_;
    kstring name_;
    virtual void do_print(out_stream& os) const override {
        os << op_ << " " << name_.begin();
    }
    virtual opcode do_opcode() const override {
        return op_;
    }
    virtual value do_eval(context&) const override {
        REQUIRE(op_ == opcode::string);
        return value{name_};
    }
};

node_ptr make_text_node(opcode op, const char* name) {
    return node_ptr{knew<text_node>(op, name).release()};
}

node_ptr make_text_node(opcode op, const kstring& s) {
   return make_text_node(op, s.begin());
}

class const_node : public node {
public:
    template<typename T>
    explicit const_node(T value) : value_(value), size_(sizeof(T)) {
    }
private:
    uint64_t value_;
    uint8_t  size_;

    virtual void do_print(out_stream& os) const override {
        os << "0x" << as_hex(value_).width(2*size_);
    }

    virtual opcode do_opcode() const override {
        return opcode::constant;
    }

    virtual value do_eval(context&) const override {
        return value{value_, static_cast<uint8_t>(size_*8)};
    }
};
template<typename T>
node_ptr make_const_node(T value) {
    return node_ptr{knew<const_node>(value).release()};
}


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

    virtual opcode do_opcode() const override {
        return opcode::buffer;
    }
};

int indent = 0;
void do_newline(out_stream& os) {
    os << "\n";
    write_many(os, ' ', indent*2);
}

class node_container {
public:
    explicit node_container() = default;
    explicit node_container(kvector<node_ptr>&& elements) : elements_(std::move(elements)) {
    }

    kvector<node_ptr>& elements() { return elements_; }

    friend out_stream& operator<<(out_stream& os, const node_container& n) {
        os << "[";
        ++indent;
        for (const auto& p : n.elements_) {
            do_newline(os);
            os << *p;
        }
        --indent;
        do_newline(os);
        os << "]";
        return os;
    }
private:
    kvector<node_ptr> elements_;
};
using node_container_ptr = kowned_ptr<node_container>;

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
        REQUIRE(!data_.empty());
        return *data_.begin();
    }

    opcode peek_opcode() const {
        const uint8_t first_byte = peek();
        if (static_cast<opcode>(first_byte) == opcode::ext_prefix) {
            REQUIRE(data_.size() >= 2);
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

    uint16_t consume_word() {
        return *reinterpret_cast<const uint16_t*>(consume(2).begin());
    }

    uint32_t consume_dword() {
        return *reinterpret_cast<const uint32_t*>(consume(4).begin());
    }

    uint64_t consume_qword() {
        return *reinterpret_cast<const uint64_t*>(consume(8).begin());
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
        if (!bindings_.empty()) {
            int n=0;
            for (const auto& b : bindings_) {
                dbgout() << b.name.begin() << " still bound to " << *b.n << "\n";
                if (++n == 10) {
                    dbgout() << bindings_.size()-n << " left\n";
                    break;
                }
            }
        }
    }

    struct binding {
        kstring name;
        node*   n;
    };
    class scope_registration {
    public:
        scope_registration(const scope_registration&) = delete;
        scope_registration& operator=(const scope_registration&) = delete;
        scope_registration(scope_registration&& other) : ns_(other.ns_), old_scope_(std::move(other.old_scope_)), node_(other.node_) {
            other.node_ = ignore_node;
        }

        node_ptr mark_dead() {
            class dead_scope_node : public node {
            public:
                explicit dead_scope_node() {}
            private:
                virtual opcode do_opcode() const override { return opcode::dead_scope; }
            };
            node_ptr dead_scope{knew<dead_scope_node>().release()};
            provide(*dead_scope);
            return dead_scope;
        }

        bool ignored() {
            return node_ == ignore_node;
        }

        void provide(node& node) {
            REQUIRE(!node_);
            std::swap(ns_.cur_namespace_, old_scope_);
            if (node.op() != opcode::dead_scope) {
                node_ = &node;
                old_scope_.push_back('\0');
                //dbgout() << "close_scope " << old_scope_.begin() << "\n";
                ns_.close_scope(std::move(old_scope_), node);
            } else {
                node_ = ignore_node;
            }
        }

        kstring name() const {
            REQUIRE(node_ != ignore_node && node_);
            auto b = ns_.find_binding(node_);
            return b->name;
        }

        ~scope_registration() {
            if (!node_) {
                old_scope_.push_back('\0');
                dbgout() << "No binding provided for " << old_scope_.begin() << "\n";
                REQUIRE(false);
            } else if (node_ != ignore_node) {
                auto b = ns_.find_binding(node_);
                REQUIRE(b);
                //dbgout() << "Erasing " << b->name.begin() << "\n";
                ns_.bindings_.erase(const_cast<binding*>(b));
            }
        }
    private:
        friend name_space;
        name_space& ns_;
        kstring     old_scope_;
        node*       node_ = nullptr;
        constexpr static node* ignore_node = (node*)42;
        explicit scope_registration(name_space& ns, kstring&& old_scope) : ns_(ns), old_scope_(std::move(old_scope)) {
        }
    };

    scope_registration open_scope(const char* name, opcode op) {
        auto old_scope = cur_namespace_;
#if 0
        dbgout() << "open_scope " << name << " " << op << " in " << hack_cur_name() << "\n";
#endif
        (void)op;
        REQUIRE(name[0] != parent_prefix_char);
        if (name[0] == root_char) {
            ++name;
            cur_namespace_.clear();
        }
        REQUIRE(string_length(name) % 4 == 0);
        cur_namespace_.insert(cur_namespace_.end(), name, name + string_length(name));
        return scope_registration{*this, std::move(old_scope)};
    }

    node* lookup_node(const char* name) const {
        if (auto b = lookup_binding(name)) {
            return b->n;
        }
        //dbgout() << "failing lookup_node(" << name << ") in " << hack_cur_name() << "\n";
        return nullptr;
    }

    node* lookup_node_in_scope(const char* name) const {
        REQUIRE(name[0] != root_char && name[1] != parent_prefix_char && !name[5]);
        if (auto b = find_binding(relative(cur_namespace_, name).begin())) {
            return b->n;
        }
        //dbgout() << "failing lookup_node_in_scope(" << name << ") in " << hack_cur_name() << "\n";
        return nullptr;
    }

    const kvector<binding>& bindings() const {
        return bindings_;
    }

    class scope_iterator {
    public:
        scope_iterator& operator++() {
            ++pos_;
            move_next();
            return *this;
        }

        bool operator==(const scope_iterator& rhs) const {
            REQUIRE(ns_ == rhs.ns_);
            return pos_ == rhs.pos_;
        }

        bool operator!=(const scope_iterator& rhs) const {
            return !(*this == rhs);
        }

        binding* operator->() const {
            return &ns_->bindings_[pos_];
        }

        binding& operator*() const {
            return ns_->bindings_[pos_];
        }

    private:
        name_space* ns_;
        kstring     prefix_;
        size_t      pos_;
        friend class name_space;

        void move_next() {
            for (; pos_ < ns_->bindings_.size(); ++pos_) {
                const auto& n = this->operator*().name;
                if (n.size() == prefix_.size() + 4 && memcmp(n.begin(), prefix_.begin(), prefix_.size()-1) == 0) {
                    break;
                }
            }
        }

        explicit scope_iterator(name_space& ns, kstring&& prefix, size_t pos) : ns_(&ns), prefix_(std::move(prefix)), pos_(pos) {
            move_next();
        }
    };

    scope_iterator scope_iter_begin(const char* prefix) {
        REQUIRE(string_length(prefix) % 4 == 0);
        return scope_iterator{*this, make_kstring(prefix), 0};
    }

    scope_iterator scope_iter_end() {
        return scope_iterator{*this, kstring(), bindings_.size()};
    }

private:
    kstring          cur_namespace_; // Not NUL-terimnated!
    kvector<binding> bindings_;

    constexpr static binding* dead_scope_binding = (binding*)42;

    const binding* lookup_binding(const char* name) const {
        // (1) ABCD      -- search rules apply
        // (2) ^ABCD     -- search rules do not apply
        // (3) XYZ.ABCD  -- search rules do not apply
        // (4) \XYZ.ABCD -- search rules do not apply
        if (name[0] == root_char) { // (4)
            if (auto* b = find_binding(name+1)) {
                return b;
            }
        } else if (name[0] == parent_prefix_char) { // (2)
            auto ns = cur_namespace_;
            for (; *name == parent_prefix_char; ++name) {
                REQUIRE(parent_scope(ns));
            }
            if (auto* b = find_binding(name)) {
                return b;
            }
        } else if (name[4]) { // (3)
            if (auto* b = find_binding(name)) {
                return b;
            }
        } else { // (1)
            auto ns = cur_namespace_;
            for (;;) {
                const auto rn = relative(ns, name);
                //dbgout() << "Trying " << rn.begin() << "\n";
                if (auto* b = find_binding(rn.begin())) {
                    return b;
                }
                if (!parent_scope(ns)) {
                    break;
                }
            }
        }
        return nullptr;
    }

    const binding* find_binding(const char* name) const {
        for (const auto& b : bindings_) {
            if (string_equal(name, b.name.begin())) {
                return &b;
            }
        }
        return nullptr;
    }

    const binding* find_binding(const node* n) const {
        for (const auto& b : bindings_) {
            if (b.n == n) {
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

    void close_scope(kstring&& name, node& n) {
        if (auto old = find_binding(name.begin())) {
            dbgout() << "Merge\n" << *old->n << "\nwith\n" << n << "\n";
            dbgout() << "Not implemented: Merge " << old->n->op() << " with " << n.op() << "\n";
            dbgout() << "Name = " << name.begin() << "\n";
            REQUIRE(false);
        }
        //dbgout() << "Registering " << name.begin() << " " << n.op() << "\n";
        bindings_.push_back(binding{std::move(name), &n});
    }
};
using scope_reg = name_space::scope_registration;

template<typename T>
struct parse_result {
    parse_state data;
    T           result;

    explicit operator bool() const {
        return static_cast<bool>(result);
    }
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
            len |= data.consume_word() << 4;
            break;
        default:
            dbgout()<<"TODO: Handle " << (b0>>6) << " extra PkgLength bytes\n";
            REQUIRE(false);
    }
    return parse_result<uint32_t>{data, len};
}

parse_state adjust_with_pkg_length(parse_state data)
{
    const uint8_t pkg_length_bytes = 1+(static_cast<uint8_t>(data.peek())>>6);
    auto pkg_len = parse_pkg_length(data);
    REQUIRE(pkg_len.result >= pkg_length_bytes);
    REQUIRE(pkg_len.result <= data.size());
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

constexpr bool is_arg_or_local(char c) {
    return c >= static_cast<char>(opcode::local_0) && c <= static_cast<char>(opcode::arg_6);
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

parse_result<node_ptr> parse_arg_or_local(parse_state data)
{
    const auto op = data.consume_opcode();
    REQUIRE(!is_extended(op) && is_arg_or_local(static_cast<char>(op)));
    return make_parse_result(data, make_simple_node(op));
}

constexpr bool is_type6_opcode(opcode op) {
    // Type6Opcode := DefRefOf | DefDerefOf | DefIndex | UserTermObj
    return op == opcode::deref_of
        || op == opcode::index;
}

parse_result<node_ptr> parse_type6_opcode(parse_state data);

parse_result<node_ptr> parse_super_name(parse_state data)
{
    // SimpleName := NameString | ArgObj | LocalObj
    // SuperName := SimpleName | DebugObj | Type6Opcode
    const uint8_t first = data.peek();
    if (is_arg_or_local(first)) {
        return parse_arg_or_local(data);
    } else if (is_name_string_start(first)) {
        auto name = parse_name_string(data);
        return make_parse_result(name.data, make_text_node(opcode::name_lookup, name.result));
    } else if (is_type6_opcode(static_cast<opcode>(first))) {
        return parse_type6_opcode(data);
    }
    dbgout() << "First = " << as_hex(first) << "\n";
    hexdump(dbgout(), data.begin()-10, std::min(32ULL, data.size()+10));
    REQUIRE(false);
}

parse_result<node_container_ptr> parse_term_list(parse_state data);
parse_result<node_ptr> parse_data_ref_object(parse_state data);
parse_result<node_ptr> parse_term_arg(parse_state data);

template<typename P>
parse_result<node_container_ptr> parse_list(parse_state state, P p)
{
    kvector<node_ptr> objs;
    while (state.begin() != state.end()) {
        auto res = p(state);
        state = res.data;
        if (!res) {
            return {state, node_container_ptr{}};
        }
        if (res.result->op() != opcode::dead_scope) {
            objs.push_back(std::move(res.result));
        }
    }
    return {state, knew<node_container>(std::move(objs))};
}

constexpr bool is_data_object(opcode op) {
    return op == opcode::zero
        || op == opcode::one
        || op == opcode::ones
        || op == opcode::byte
        || op == opcode::word
        || op == opcode::dword
        || op == opcode::string
        || op == opcode::qword
        || op == opcode::buffer
        || op == opcode::package;
}

class package_node : public node {
public:
    explicit package_node(node_container_ptr&& nodes) : nodes_(std::move(nodes)) {
    }

private:
    node_container_ptr nodes_;

    virtual void do_print(out_stream& os) const override {
        os << "Package " << *nodes_;
    }
    virtual opcode do_opcode() const override {
        return opcode::package;
    }
};

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
                const uint16_t w = data.consume_word();
                return make_parse_result(data, make_const_node(w));
            }
        // DwordConst
        case opcode::dword:
            {
                const uint32_t d = data.consume_dword();
                return make_parse_result(data, make_const_node(d));
            }
        case opcode::qword:
            {
                const uint64_t q = data.consume_qword();
                return make_parse_result(data, make_const_node(q));
            }
        case opcode::string:
            {
                const char* text = reinterpret_cast<const char*>(data.begin());
                data.consume(static_cast<uint32_t>(string_length(text) + 1));
                return make_parse_result(data, make_text_node(op, text));
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
                auto list = parse_list(pkg_data, [](parse_state data) -> parse_result<node_ptr> {
                    if (is_name_string_start(data.peek())) {
                        auto name = parse_name_string(data);
                        //dbgout() << "PackageElement NameString " << name.result.begin() << "\n";
                        return make_parse_result(name.data, make_text_node(opcode::package_element, name.result));
                    } else {
                        return parse_data_ref_object(data);
                        //dbgout() << "PackageElement DataRefObject " << *data_ref_object.result << "\n";
                    }
                });
                REQUIRE(list.result);
                auto& elements = list.result->elements();
                //dbgout()<<"elements.size() = " << elements.size() << "\n";
                //dbgout()<<"num_elements = " << num_elements << "\n";
                REQUIRE(elements.empty() || num_elements == elements.size()); // Local0 = Package(0x02){} is legal ==> PackageElementList is empty
                return make_parse_result(data.moved_to(pkg_data.end()), knew<package_node>(std::move(list.result)));
            }
        default:
            hexdump(dbgout(), data.begin()-2, std::min(data.size()+2, 64ULL));
            dbgout() << "Unhandled opcode " << op << "\n";
            REQUIRE(false);
    }
}

parse_result<node_ptr> parse_target(parse_state data) {
    // Target := SuperName | NullName
    if (data.peek() == 0) {
        data.consume();
        return make_parse_result(data, make_simple_node(opcode::null_name));
    } else {
        return parse_super_name(data);
    }
}

template<int arity>
class op_node_base : public node {
public:
    template<typename... Args>
    explicit op_node_base(const char* name, Args&&... args) : name_(make_kstring(name)), args_{std::move(args)...} {
        static_assert(sizeof...(Args) == arity || sizeof...(Args) == arity + 1, "");
    }
    // public for knew.. because lazy
    explicit op_node_base(const char* name, node_ptr (&args)[arity+1]) : name_(make_kstring(name)) {
        for (int i = 0; i < arity+1; ++i) {
            args_[i] = std::move(args[i]);
        }
    }


    static parse_result<node_ptr> parse(parse_state state, const char* name, bool target) {
        node_ptr args_[arity+1];
        for (int i = 0; i < arity; ++i) {
           if (auto arg = parse_term_arg(state)) {
               args_[i] = std::move(arg.result);
               state = arg.data;
           } else {
               return arg;
           }
        }
        if (target) {
            auto tgt = parse_target(state);
            args_[arity] = std::move(tgt.result);
            state = tgt.data;
        }
        return make_parse_result(state, knew<op_node_base>(name, args_));
    }

private:
    kstring  name_;
    node_ptr args_[arity+1]; // last is target (could be null)

    virtual void do_print(out_stream& os) const override {
        os << name_.begin();
        for (int i = 0; i < arity; ++i) {
            os << " " << *args_[i];
        }
        if (args_[arity] && args_[arity]->op() != opcode::null_name) {
            os << " " << *args_[arity];
        }
    }
};

using unary_op_node = op_node_base<1>;
using binary_op_node = op_node_base<2>;

class method_node : public node {
public:
    explicit method_node(scope_reg&& ns_reg, kstring&& name, uint8_t flags, node_container_ptr&& statements) : reg_(std::move(ns_reg)), name_(std::move(name)), flags_(flags), statements_(std::move(statements)) {
        reg_.provide(*this);
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
    const char* name() const { return name_.begin(); }

private:
    scope_reg      reg_;
    kstring        name_;
    uint8_t        flags_;
    node_container_ptr statements_;
    virtual void do_print(out_stream& os) const override {
        os << "Method " << name_.begin() << " Flags " << as_hex(flags_) << " ";
        if (statements_) {
            os << *statements_;
        } else {
            os << "[Parse failed]";
        }
    }

    virtual opcode do_opcode() const override {
        return opcode::method;
    }

    virtual value do_eval(context&) const override {
        dbgout() << "Not calling " << *this << "\n";
        return value{};
    }
};

class name_node : public node {
public:
    explicit name_node(scope_reg&& ns_reg, kstring&& name, node_ptr&& obj) : reg_(std::move(ns_reg)), name_(std::move(name)), obj_(std::move(obj)) {
        reg_.provide(*this);
        REQUIRE(obj_);
    }

    const node& obj() const {
        return *obj_;
    }

private:
    scope_reg   reg_;
    kstring     name_;
    node_ptr    obj_;
    virtual void do_print(out_stream& os) const override {
        os << "Name " << name_.begin() << " " << *obj_;
    }

    virtual opcode do_opcode() const override {
        return opcode::name;
    }

    virtual value do_eval(context& ctx) const override {
        return obj_->eval(ctx);
    }
};

parse_result<node_ptr> parse_unary_op(parse_state data, const char* name) {
    return unary_op_node::parse(data, name, true);
}

parse_result<node_ptr> parse_unary_op_no_target(parse_state data, const char* name) {
    return unary_op_node::parse(data, name, false);
}

parse_result<node_ptr> parse_binary_op(parse_state data, const char* name) {
    return binary_op_node::parse(data, name, true);
}

parse_result<node_ptr> parse_logical_op(parse_state data, const char* name) {
    return binary_op_node::parse(data, name, false);
}

parse_result<node_ptr> parse_index(parse_state data)
{
    // DefIndex := IndexOp BuffPkgStrObj IndexValue Target
    // BuffPkgStrObj := TermArg => Buffer, Package or String
    // IndexValue := TermArg => Integer
    return parse_binary_op(data, "index");
}

parse_result<node_ptr> parse_type6_opcode(parse_state data) {
    // Type6Opcode := DefRefOf | DefDerefOf | DefIndex | UserTermObj
    const auto op = data.consume_opcode();
    REQUIRE(is_type6_opcode(op));
    REQUIRE(op == opcode::index);
    return parse_index(data);
}

class method_call_node : public node {
public:
    explicit method_call_node(method_node& method, node_container_ptr&& args) : method_(method), args_(std::move(args)) {
    }

private:
    method_node&       method_;
    node_container_ptr args_;
    virtual void do_print(out_stream& os) const override {
        os << method_.name() << "(" << *args_ << ")";
    }

    virtual opcode do_opcode() const override {
        return opcode::method_call;
    }
};

constexpr bool is_type2_opcode(opcode op) {
    return op == opcode::add
        || op == opcode::concat
        || op == opcode::subtract
        || op == opcode::increment
        || op == opcode::decrement
        || op == opcode::multiply
        || op == opcode::divide
        || op == opcode::shift_left
        || op == opcode::shift_right
        || op == opcode::and_
        || op == opcode::or_
        || op == opcode::xor_
        || op == opcode::not_
        || op == opcode::find_set_left_bit
        || op == opcode::find_set_right_bit
        || op == opcode::deref_of
        || op == opcode::size_of
        || op == opcode::index
        || op == opcode::object_type
        || op == opcode::land
        || op == opcode::lor
        || op == opcode::lnot
        || op == opcode::lequal
        || op == opcode::lgreater
        || op == opcode::lless
        || op == opcode::mid
        || op == opcode::cond_ref_of
        || op == opcode::sleep
        || op == opcode::acquire
        || op == opcode::release
        || (static_cast<unsigned>(op) <= 0xff && is_name_string_start(static_cast<uint8_t>(op))); // Method invocation
}

parse_result<node_ptr> parse_type2_opcode(parse_state data) {
    if (is_name_string_start(data.peek())) { // Method invocation
        // It seems NameSeg is valid here. E.g. ReturnOp 'TMP_'
        // Is this sometimes a method invocation?
        auto name = parse_name_string(data);
        data = name.data;
        if (auto n = data.ns().lookup_node(name.result.begin())) {
            if (n->op() != opcode::method) {
                return make_parse_result(data, make_text_node(opcode::name_lookup, name.result));
            }
            auto& method = static_cast<method_node&>(*n);
            kvector<node_ptr> args;
            for (int i = 0; i < method.arg_count(); ++i) {
                auto arg = parse_term_arg(data);
                if (!arg) return arg;
                args.push_back(std::move(arg.result));
                data = arg.data;
            }
            return make_parse_result(data, knew<method_call_node>(method, knew<node_container>(std::move(args))));
        }
        return {data, node_ptr{}};
    }

    const auto op = data.consume_opcode();
    if (!is_type2_opcode(op)) {
        hexdump(dbgout(), data.begin(), std::min(32ULL, data.size()));
        dbgout() << "op = " << op << "\n";
        REQUIRE(false);
    }
    switch (op) {
        case opcode::add:                return parse_binary_op(data, "add");
        case opcode::concat:             return parse_binary_op(data, "concat");
        case opcode::subtract:           return parse_binary_op(data, "subtract");
        case opcode::increment:          return parse_unary_op_no_target(data, "increment");
        // DefDecrement := DecrementOp SuperName
        case opcode::decrement:          return parse_unary_op_no_target(data, "decrement");
        case opcode::divide:
            {
                // DefDivide := DivideOp Dividend Divisor Remainder Quotient
                // Dividend := TermArg => Integer
                // Divisor := TermArg => Integer
                // Remainder := Target
                // Quotient := Target
                auto dividend  = parse_term_arg(data);
                auto divisor   = parse_term_arg(dividend.data);
                auto remainder = parse_target(divisor.data);
                auto quotient  = parse_target(remainder.data);
                // TOOD: handle remainder
                return make_parse_result(quotient.data, knew<binary_op_node>("divide", std::move(dividend.result), std::move(divisor.result), std::move(quotient.result)));
            }
        case opcode::multiply:           return parse_binary_op(data, "multiply");
        case opcode::shift_left:         return parse_binary_op(data, "shiftleft");
        case opcode::shift_right:        return parse_binary_op(data, "shiftright");
        case opcode::and_:               return parse_binary_op(data, "and");
        case opcode::or_:                return parse_binary_op(data, "or");
        case opcode::xor_:               return parse_binary_op(data, "xor");
        case opcode::not_:               return parse_unary_op(data, "not");
        case opcode::find_set_left_bit:  return parse_unary_op(data, "find_set_left_bit");
        case opcode::find_set_right_bit: return parse_unary_op(data, "find_set_right_bit");
        case opcode::deref_of:
            {
                auto arg = parse_term_arg(data);
                // DefDerefOf := DerefOfOp ObjReference
                // ObjReference := TermArg => ObjectReference | String
                return make_parse_result(arg.data, knew<unary_op_node>("deref_of", std::move(arg.result)));
            }
        case opcode::size_of:            return parse_unary_op_no_target(data, "sizeof");
        case opcode::index:              return parse_index(data);
        // DefObjectType := ObjectTypeOp <SimpleName | DebugObj | DefRefOf | DefDerefOf | DefIndex>
        case opcode::object_type:        return parse_unary_op_no_target(data, "ObjectType");
        case opcode::land:               return parse_logical_op(data, "land");
        case opcode::lor:                return parse_logical_op(data, "lor");
        case opcode::lnot:
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
                auto arg = parse_term_arg(data);
                if (!arg) {
                    return arg;
                }
                return make_parse_result(arg.data, knew<unary_op_node>("lnot", std::move(arg.result)));
            }
        case opcode::lequal:             return parse_logical_op(data, "lequal");
        case opcode::lgreater:           return parse_logical_op(data, "lgreater");
        case opcode::lless:              return parse_logical_op(data, "lless");
        case opcode::mid:
            {
                // DefMid := MidOp MidObj TermArg TermArg Target
                // MidObj := TermArg => Buffer | String
                return op_node_base<3>::parse(data, "mid", true);
            }
        case opcode::cond_ref_of:
            {
                // DefCondRefOf := CondRefOfOp SuperName Target
                auto name   = parse_super_name(data);
                auto target = parse_target(name.data);
                //dbgout() << "CondRefOf " << name.result.begin() << " -> " << target.result.begin() << "\n";
                return make_parse_result(target.data, knew<unary_op_node>("cond_ref_of", std::move(name.result), std::move(target.result)));
            }
            break;
        case opcode::sleep:
            {
                // DefSleep := SleepOp MsecTime (:= TermArg => Integer)
                auto msectime = parse_term_arg(data);
                return make_parse_result(msectime.data, knew<unary_op_node>("sleep", std::move(msectime.result)));
            }
        case opcode::acquire:
            {
                // DefAcquire := AcquireOp MutexObject (:= SuperName) Timeout (:= WordData)
                auto arg = parse_super_name(data);
                const auto timeout = arg.data.consume_word();
                return make_parse_result(arg.data, knew<binary_op_node>("acquire", std::move(arg.result), make_const_node(timeout)));
            }
        case opcode::release:
            {
                // DefRelease := ReleaseOp MutexObject (:= SuperName)
                auto arg = parse_super_name(data);
                return make_parse_result(arg.data, knew<unary_op_node>("release", std::move(arg.result)));
            }
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
        return parse_arg_or_local(data);
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
    explicit named_field_node(scope_reg&& ns_reg, kstring&& name, uint32_t len) : reg_(std::move(ns_reg)), name_(std::move(name)), len_(len) {
        reg_.provide(*this);
    }

private:
    scope_reg reg_;
    kstring   name_;
    uint32_t  len_;

    virtual void do_print(out_stream& os) const override {
        os << "NamedField " << name_.begin() << " Length " << as_hex(len_).width(0);
    }

    virtual opcode do_opcode() const override {
        return opcode::named_field;
    }
};

class field_node : public node {
public:
    explicit field_node(kstring&& name, uint8_t flags, node_container_ptr&& fields) : name_(std::move(name)), flags_(flags), fields_(std::move(fields)) {
    }

private:
    kstring        name_;
    uint8_t        flags_;
    node_container_ptr fields_;

    virtual void do_print(out_stream& os) const override {
        os << "Field " << name_.begin() << " Flags " << as_hex(flags_) << " " << *fields_;
    }

    virtual opcode do_opcode() const override {
        return opcode::field;
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
    auto scope_reg = data.ns().open_scope(name.begin(), opcode::named_field);
    return make_parse_result(len.data, knew<named_field_node>(std::move(scope_reg), std::move(name), len.result));
}

parse_result<node_container_ptr> parse_field_list(parse_state data)
{
    // FieldList := Nothing | <FieldElement FieldList>
    kvector<node_ptr> elements;
    // TODO: Use parse_list
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
            elements.push_back(make_simple_node(opcode::reserved_field));
            data = len.data;
        } else if (first == 1) {
            data.consume();
            const uint8_t access_type   = data.consume();
            const uint8_t access_attrib = data.consume();
            elements.push_back(make_simple_node(opcode::access_field));
        } else if (is_lead_name_char(first)) {
            auto named_field = parse_named_field(data);
            elements.push_back(std::move(named_field.result));
            data = named_field.data;
        } else {
            dbgout() << "Unknown FieldElement " << as_hex(first) << "\n";
            REQUIRE(false);
        }
    }
    return {data, knew<node_container>(std::move(elements))};
}

parse_result<node_container_ptr> parse_object_list(parse_state data);

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

class if_node : public node {
public:
    explicit if_node(node_ptr&& predicate, node_container_ptr&& if_statements, node_container_ptr&& else_statements)
        : predicate_(std::move(predicate))
        , if_statements_(std::move(if_statements))
        , else_statements_(std::move(else_statements)) {
    }

private:
    node_ptr       predicate_;
    node_container_ptr if_statements_;
    node_container_ptr else_statements_;
    virtual void do_print(out_stream& os) const override {
        os << "If (" << *predicate_ << ") { " << *if_statements_ << "}";
        if (else_statements_) {
            os << " Else { " << *else_statements_ << "}";
        }
    }
};

class while_node : public node {
public:
    explicit while_node(node_ptr&& predicate, node_container_ptr&& statements)
        : predicate_(std::move(predicate))
        , statements_(std::move(statements)) {
    }

private:
    node_ptr       predicate_;
    node_container_ptr statements_;
    virtual void do_print(out_stream& os) const override {
        os << "While (" << *predicate_ << ") { " << *statements_ << "}";
    }
};

class mutex_node : public node {
public:
    explicit mutex_node(kstring&& name, uint8_t flags) : name_(std::move(name)), flags_(flags) {
        REQUIRE((flags & 0xf0) == 0x00);
    }

private:
    kstring name_;
    uint8_t flags_;
    virtual void do_print(out_stream& os) const override {
        os << "Mutex " << name_.begin() << " SyncLevel " << as_hex(flags_).width(1);
    }
};

class create_field_node : public node {
public:
    explicit create_field_node(scope_reg&& reg, opcode type, kstring&& name, node_ptr&& buffer, node_ptr&& index)
        : reg_(std::move(reg))
        , type_(type)
        , name_(std::move(name))
        , buffer_(std::move(buffer))
        , index_(std::move(index)) {
        reg_.provide(*this);
    }
private:
    scope_reg reg_;
    opcode    type_;
    kstring   name_;
    node_ptr  buffer_;
    node_ptr  index_;
    virtual void do_print(out_stream& os) const override {
        // DefCreateBitField   := CreateBitFieldOp SourceBuff BitIndex NameString
        // DefCreateDWordField := CreateDWordFieldOp SourceBuff ByteIndex NameString
        // DefCreateQWordField := CreateQWordFieldOp SourceBuff ByteIndex NameString
        // DefCreateWordField  := CreateWordFieldOp SourceBuff ByteIndex NameString
        os << type_ << " " << name_.begin() << " " << *buffer_ << " " << *index_;
    }
    virtual opcode do_opcode() const override {
        return type_;
    }
};

class container_node : public node {
public:
    explicit container_node(scope_reg&& reg, opcode op, node_container_ptr&& nodes) : reg_(std::move(reg)), op_(op), nodes_(std::move(nodes)) {
        if (!reg_.ignored()) {
            reg_.provide(*this);
        }
    }

    ~container_node() {
        REQUIRE(prev_ == nullptr || reg_.ignored());
        if (prev_) {
            REQUIRE(prev_->next_ == this);
            prev_->next_ = next_;
        }
        if (next_) {
            REQUIRE(next_->prev_ == this);
            next_->prev_ = prev_;
        }
    }

    void merge(node& other_) {
        REQUIRE(other_.is_container());
        REQUIRE(other_.op() == opcode::scope || other_.op() == opcode::device);
        REQUIRE(op() == opcode::scope || op() == opcode::device);
        auto& other = static_cast<container_node&>(other_);
        REQUIRE(!prev_);
        REQUIRE(!other.prev_);
        REQUIRE(!other.next_);
        container_node* before = this;
        while (before->next_ != nullptr) {
            before = before->next_;
        }
        before->next_ = &other;
        other.prev_   = before;
    }

private:
    scope_reg          reg_;
    opcode             op_;
    node_container_ptr nodes_;
    container_node*    next_ = nullptr;
    container_node*    prev_ = nullptr;

    virtual void do_print(out_stream& os) const override {
        if (prev_) {
            os << "Helper " << op_;
            return;
        }
        os << op_ << " " << reg_.name().begin() << " " << *nodes_;
        for (auto* c = next_; c; c = c->next_) {
            os << " " << *c->nodes_;
        }
    }
    virtual opcode do_opcode() const override {
        return op_;
    }

    virtual bool do_is_container() const override {
        return true;
    }
};

node_ptr make_container_node(scope_reg&& ns_reg, opcode op, node_container_ptr&& nodes) {
    return node_ptr{knew<container_node>(std::move(ns_reg), op, std::move(nodes)).release()};
}


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
                auto scope_reg       = data.ns().open_scope(name.result.begin(), op);
                auto data_ref_object = parse_data_ref_object(name.data);
                //dbgout() << "DefName " << name.result.begin() << " " << *data_ref_object.result << "\n";
                return make_parse_result(data_ref_object.data, knew<name_node>(std::move(scope_reg), std::move(name.result), std::move(data_ref_object.result)));
            }
        case opcode::scope:
            {
                // DefScope := ScopeOp PkgLength NameString TermList
                auto pkg_data  = adjust_with_pkg_length(data);
                auto name      = parse_name_string(pkg_data);
                auto scope_reg = data.ns().open_scope(name.result.begin(), op);
                auto term_list = parse_term_list(name.data);
                REQUIRE(term_list);
                REQUIRE(pkg_data.end() == term_list.data.begin());
                node* n = data.ns().lookup_node(name.result.begin());
                if (n) {
                    scope_reg.mark_dead();
                }
                auto scope_node = make_container_node(std::move(scope_reg), op, std::move(term_list.result));
                if (n) {
                    REQUIRE(n->is_container());
                    static_cast<container_node&>(*n).merge(*scope_node);
                }
                return make_parse_result(data.moved_to(pkg_data.end()), std::move(scope_node));
            }
        case opcode::method:
            {
                // DefMethod := MethodOp PkgLength NameString MethodFlags TermList
                auto pkg_data = adjust_with_pkg_length(data);
                auto name = parse_name_string(pkg_data);
                auto scope_reg = data.ns().open_scope(name.result.begin(), op);
                pkg_data = name.data;
                uint8_t method_flags = pkg_data.consume();
                node_container_ptr term_list;
                auto term_list_parsed = parse_term_list(pkg_data);
                if (term_list_parsed) {
                    data = data.moved_to(term_list_parsed.data.begin());
                    term_list = std::move(term_list_parsed.result);
                } else {
                    data = data.moved_to(pkg_data.end());
                }
                //dbgout() << "DefMethod " << name.result.begin() << " Flags " << as_hex(method_flags) << "\n";
                return make_parse_result(data, knew<method_node>(std::move(scope_reg), std::move(name.result), method_flags, std::move(term_list)));
            }
        case opcode::store:
            {
                // DefStore := StoreOp TermArg SuperName
                auto arg  = parse_term_arg(data);
                if (!arg) return arg;
                auto name = parse_super_name(arg.data);
                //dbgout() << "Store " << *arg.result << " -> " << name.result.begin() << "\n";
                return make_parse_result(name.data, knew<unary_op_node>("store", std::move(arg.result), std::move(name.result)));
            }
        case opcode::notify:
            {
                // DefNotify := NotifyOp NotifyObject NotifyValue
                // NotifyObject := SuperName => ThermalZone | Processor | Device
                // NotifyValue := TermArg => Integer
                auto obj   = parse_super_name(data);
                auto value = parse_term_arg(obj.data);
                return make_parse_result(value.data, knew<unary_op_node>("notify", std::move(value.result), std::move(obj.result)));
            }
        case opcode::create_dword_field:
        case opcode::create_word_field:
        case opcode::create_byte_field:
        case opcode::create_bit_field:
        case opcode::create_qword_field:
            {
                // DefCreateBitField   := CreateBitFieldOp SourceBuff BitIndex NameString
                // DefCreateByteField  := CreateByteFieldOp SourceBuff ByteIndex NameString
                // DefCreateDWordField := CreateDWordFieldOp SourceBuff ByteIndex NameString
                // DefCreateQWordField := CreateQWordFieldOp SourceBuff ByteIndex NameString
                // DefCreateWordField  := CreateWordFieldOp SourceBuff ByteIndex NameString
                // SourceBuff          := TermArg => Buffer
                // BitIndex            := TermArg => Integer
                // ByteIndex           := TermArg => Integer

                auto source_buffer = parse_term_arg(data);
                auto byte_index    = parse_term_arg(source_buffer.data); // bit_index for CreateBitField
                auto name          = parse_name_string(byte_index.data);
                auto scope_reg     = data.ns().open_scope(name.result.begin(), op);
                return make_parse_result(name.data, knew<create_field_node>(std::move(scope_reg), op, std::move(name.result), std::move(source_buffer.result), std::move(byte_index.result)));
            }
            break;
        case opcode::if_:
            {
                // DefIfElse := IfOp PkgLength Predicate TermList DefElse
                // Predicate := TermArg => Integer
                // DefElse := Nothing | <ElseOp PkgLength TermList>
                auto pkg_data  = adjust_with_pkg_length(data);
                node_ptr predicate;
                node_container_ptr if_statements;
                node_container_ptr else_statements;
                if (auto predicate_arg = parse_term_arg(pkg_data)) {
                    predicate = std::move(predicate_arg.result);
                    if (auto term_list = parse_term_list(predicate_arg.data)) {
                        if_statements = std::move(term_list.result);
                    } else {
                        return make_parse_result(term_list.data, node_ptr{});
                    }
                } else {
                    return predicate_arg;
                }
                data = data.moved_to(pkg_data.end());
                if (data.begin() != data.end() && data.peek_opcode() == opcode::else_) {
                    data.consume_opcode();
                    auto else_pkg = adjust_with_pkg_length(data);
                    if (auto else_term_list = parse_term_list(else_pkg)) {
                        else_statements = std::move(else_term_list.result);
                    } else {
                        return make_parse_result(else_term_list.data, node_ptr{});
                    }
                    data = data.moved_to(else_pkg.end());
                }
                return make_parse_result(data, knew<if_node>(std::move(predicate), std::move(if_statements), std::move(else_statements)));
            }
        case opcode::while_:
            {
                // DefWhile := WhileOp PkgLength Predicate TermList
                auto pkg_data  = adjust_with_pkg_length(data);
                if (auto predicate = parse_term_arg(pkg_data)) {
                    if (auto statements = parse_term_list(predicate.data)) {
                        return make_parse_result(data.moved_to(pkg_data.end()), knew<while_node>(std::move(predicate.result), std::move(statements.result)));
                    } else {
                        return make_parse_result(statements.data, node_ptr{});
                    }
                } else {
                    return predicate;
                }
            }
        case opcode::noop:
            {
                return make_parse_result(data, make_simple_node(op));
            }
        case opcode::return_:
            {
                // DefReturn := ReturnOp ArgObject
                // ArgObject := TermArg => DataRefObject
                auto arg = parse_term_arg(data);
                if (!arg) return arg;
                //dbgout() << "Return " << *arg.result << "\n";
                return make_parse_result(arg.data, knew<unary_op_node>("return", std::move(arg.result)));
            }
        case opcode::mutex_:
            {
                // DefMutex := MutexOp NameString SyncFlags
                // SyncFlags := ByteData // bit 0-3: SyncLevel (0x00-0x0f), bit 4-7: Reserved (must be 0)
                auto name  = parse_name_string(data);
                auto flags = name.data.consume();
                return make_parse_result(name.data, knew<mutex_node>(std::move(name.result), flags));
            }
        case opcode::create_field:
            {
                // DefCreateField := CreateFieldOp SourceBuff BitIndex NumBits NameString
                // BitIndex := TermArg => Integer
                // NumBits := TermArg => Integer
                auto source_buffer = parse_term_arg(data);
                auto bit_index     = parse_term_arg(source_buffer.data);
                auto num_bits      = parse_term_arg(bit_index.data);
                auto name          = parse_name_string(num_bits.data);
                // Very hackish
                return make_parse_result(name.data, knew<binary_op_node>("CreateField", std::move(source_buffer.result), std::move(num_bits.result), make_text_node(opcode::name_lookup, name.result)));
            }
        case opcode::fatal:
            {
                // DefFatal := FatalOp FatalType (:= ByteData) FatalCode (:= DwordData) FatalArg (:= TermArg => Intger)
                auto type = data.consume();
                auto code = data.consume_dword();
                auto arg  = parse_term_arg(data);
                return make_parse_result(arg.data, knew<op_node_base<3>>("Fatal", make_const_node(type), make_const_node(code), std::move(arg.result)));
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
                auto pkg_data    = adjust_with_pkg_length(data);
                auto name        = parse_name_string(pkg_data);
                auto field_flags = parse_field_flags(name.data);
                //dbgout() << "DefField " << name.result.begin() << " flags " << *field_flags.result << "\n";
                auto field_list  = parse_field_list(field_flags.data);
                return make_parse_result(data.moved_to(pkg_data.end()), knew<field_node>(std::move(name.result), field_flags.result, std::move(field_list.result)));
            }
        case opcode::device:
            {
                // DefDevice := DeviceOp PkgLength NameString ObjectList
                auto pkg_data    = adjust_with_pkg_length(data);
                auto name        = parse_name_string(pkg_data);
                auto scope_reg   = data.ns().open_scope(name.result.begin(), op);
                auto object_list = parse_object_list(name.data);
                REQUIRE(object_list);
                return make_parse_result(data.moved_to(pkg_data.end()), make_container_node(std::move(scope_reg), op, std::move(object_list.result)));
            }
        case opcode::processor:
            {
                // DefProcessor := ProcessorOp PkgLength NameString ProcID (:= ByteData) PblkAddr (:= DWordData) PblkLen (:= ByteData) ObjectList
                auto pkg_data        = adjust_with_pkg_length(data);
                auto name            = parse_name_string(pkg_data);
                const auto proc_id   = name.data.consume();
                const auto pblk_addr = name.data.consume_dword();
                const auto pblk_len  = name.data.consume();
                auto scope_reg       = data.ns().open_scope(name.result.begin(), op);
                auto objs            = parse_object_list(name.data);
                REQUIRE(objs);
                //dbgout() << "Processor " << name.result.begin() << " Id " << as_hex(proc_id) << " Addr " << as_hex(pblk_addr) << " Len " << as_hex(pblk_len) << "\n";
                return make_parse_result(data.moved_to(pkg_data.end()), make_container_node(std::move(scope_reg), op, std::move(objs.result)));
            }
        case opcode::power_res:
            {
                // DefPowerRes := PowerResOp PkgLength NameString SystemLevel (:= ByteData) ResourceOrder (:= WordData) ObjectList
                auto pkg_data        = adjust_with_pkg_length(data);
                auto name            = parse_name_string(pkg_data);
                const auto sys_lvl   = name.data.consume();
                const auto res_order = name.data.consume_word();
                auto scope_reg       = data.ns().open_scope(name.result.begin(), op);
                auto objs            = parse_object_list(name.data);
                REQUIRE(objs);
                //dbgout() << "PowerRes " << name.result.begin() << " SystemLevel " << as_hex(sys_lvl) << " ResourceOrder " << as_hex(res_order) << "\n";
                return make_parse_result(data.moved_to(pkg_data.end()), make_container_node(std::move(scope_reg), op, std::move(objs.result)));
            }
        case opcode::thermal_zone:
            {
                // DefThermalZone := ThermalZoneOp PkgLength NameString ObjectList
                auto pkg_data        = adjust_with_pkg_length(data);
                auto name            = parse_name_string(pkg_data);
                auto scope_reg       = data.ns().open_scope(name.result.begin(), op);
                auto objs            = parse_object_list(name.data);
                REQUIRE(objs);
                return make_parse_result(data.moved_to(pkg_data.end()), make_container_node(std::move(scope_reg), op, std::move(objs.result)));
            }
        case opcode::index_field:
            {
                // DefIndexField := IndexFieldOp PkgLength NameString NameString FieldFlags FieldList
                // DefField := FieldOp PkgLength NameString FieldFlags FieldList
                auto name        = parse_name_string(adjust_with_pkg_length(data));
                auto name2       = parse_name_string(name.data);
                auto field_flags = parse_field_flags(name2.data);
                //dbgout() << "DefIndexField " << name.result.begin() << " " << name2.result.begin() << " flags " << field_flags.result << "\n";
                auto field_list  = parse_field_list(field_flags.data);
                return make_parse_result(data.moved_to(field_list.data.begin()), knew<field_node>(std::move(name.result), field_flags.result, std::move(field_list.result)));
            }
        default:
            dbgout() << "Unhandled opcode " << op << "\n";
            hexdump(dbgout(), data.begin()-(is_extended(op)?2:1), std::min(32ULL, data.size()));
            REQUIRE(false);
    }
}

parse_result<node_container_ptr> parse_object_list(parse_state state)
{
    // ObjectList := Nothing | <Object ObjectList>
    // Object := NameSpaceModifierObj | NamedObj
    return parse_list(state, &parse_term_obj); // TODO: Type1Opcode | Type2Opcode not allowed in ObjectList
}

parse_result<node_container_ptr> parse_term_list(parse_state state)
{
    // TermList := Nothing | <TermObj TermList>
    return parse_list(state, &parse_term_obj);
}

void recursive_print(name_space& ns, const kstring& prefix, int nested = 0)
{
    for (auto it = ns.scope_iter_begin(prefix.begin()), end = ns.scope_iter_end(); it != end; ++it) {
        write_many(dbgout(), ' ', nested*2);
        dbgout() << it->name.begin()+prefix.size()-1 << " " << it->n->op() << "\n";
        if (it->n->is_container()) {
            recursive_print(ns, it->name, nested+1);
        }
    }
}

void process(array_view<uint8_t> data)
{
    name_space ns;
    // See ACPI 6.1: 5.7.2 
    // \_OS Name of the operating system
    // \_OSI (Operating System Interfaces)
    auto os_string  = make_text_node(opcode::string, "\\_OS_");
    auto reg = ns.open_scope("\\_OS_", opcode::string);
    reg.provide(*os_string);
    auto osi_method = node_ptr{knew<method_node>(ns.open_scope("\\_OSI", opcode::method), make_kstring("\\_OSI"), uint8_t(1), knew<node_container>()).release()};
    parse_state state{ns, data};
    auto res = parse_term_list(state);
    REQUIRE(res);
#if 0
    // Printing this only prints the _parse tree_
    // things belonging in the root scope may be nested inside "helper nodes"
    dbgout() << *res.result << "\n";
#endif
#if 0
    for (const auto& b : ns.bindings()) {
        dbgout() << b.name.begin() << " " << b.n->op () << "\n";
    }
#endif
#if 0
    recursive_print(ns, make_kstring(""));
#endif

    auto concat = [](const kstring& a, const char* sf) {
        REQUIRE(!a.empty());
        REQUIRE(string_length(sf) == 4);
        auto n = a;
        n.pop_back();
        n.insert(n.end(), sf, sf + 5);
        return n;
    };
    auto print = [&ns](const kstring& name_, const char* prop) {
        REQUIRE(!name_.empty());
        REQUIRE(string_length(prop) == 4);
        auto name = name_;
        name.pop_back();
        name.insert(name.end(), prop, prop + 5);
        if (auto n = ns.lookup_node(name.begin())) {
            dbgout() << name.begin() << " ";
            context ctx{};
            auto val = n->eval(ctx);
            if (val.type() == value_type::integer) {
                if (prop[1] == 'H') { // Monster hack for _HID
                    auto uint = val.as_uint();
                    REQUIRE(uint.bits >= 16 && uint.bits <= 32);
                    //  EISA ID Definition 32-bits
                    //   bits[15:0] - three character compressed ASCII EISA ID.
                    //   bits[31:16] - binary number
                    //    Compressed ASCII is 5 bits per character 0b00001 = 'A' 0b11010 = 'Z'
                    auto swap16 = [](uint64_t val) -> uint16_t { return ((val&0xff)<<8) | ((val&0xff00)>>8); };
                    const uint16_t eisa_id = swap16(uint.val); // EISA ID is big-endian
                    const char c0 = '@' + (eisa_id & 0x1f);
                    const char c1 = '@' + ((eisa_id>>5) & 0x1f);
                    const char c2 = '@' + ((eisa_id>>10) & 0x1f);
                    dbgout() << c0 << c1 << c2 << as_hex(swap16(uint.val>>16)).width(4);
                } else {
                    dbgout() << val;
                }
            } else if (val.type() == value_type::string) {
                dbgout() << val;
            } else {
                dbgout() << "UNHANDLED: " << val;
            }
            dbgout() << "\n";
            return true;
        }
        return false;
    };

    for (const auto& b : ns.bindings()) {
        if (b.n->op() != opcode::device) continue;
        print(b.name, "_HID") || print(b.name, "_ADR");
    }
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

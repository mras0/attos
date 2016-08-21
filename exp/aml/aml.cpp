#include <attos/out_stream.h>
#include <attos/cpu.h>
#include <attos/containers.h>
#include <attos/array_view.h>
#include <attos/string.h>

namespace attos { namespace aml {

enum class opcode : uint8_t {
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
};

class __declspec(novtable) node {
public:
    virtual ~node() = 0 {}

    friend out_stream& operator<<(out_stream& os, const node& n) {
        n.do_print(os);
        return os;
    }

private:
    virtual void do_print(out_stream& os) const = 0;
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

template<typename T>
struct parse_result {
    array_view<uint8_t> data;
    T                   result;
};

template<typename Node>
parse_result<node_ptr> make_parse_result(array_view<uint8_t> data, kowned_ptr<Node>&& node) {
    return parse_result<node_ptr>{data, node_ptr{node.release()}};
}

char peek(array_view<uint8_t> data) {
    return *data.begin();
}

uint8_t consume(array_view<uint8_t>& data) {
    const auto b = *data.begin();
    data = array_view<uint8_t>(data.begin() + 1, data.end());
    return b;
}

array_view<uint8_t> advanced(array_view<uint8_t> data, uint32_t size) {
    REQUIRE(data.size() >= size);
    return array_view<uint8_t>(data.begin() + size, data.end());
}

array_view<uint8_t> with_size(array_view<uint8_t> data, uint32_t size) {
    REQUIRE(data.size() >= size);
    return array_view<uint8_t>(data.begin(), data.begin() + size);
}

auto parse_pkg_length(array_view<uint8_t> data)
{
    const uint8_t b0 = consume(data);
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
            len |= (consume(data) << 4);
            break;
        case 2:
            len = b0 & 0xf;
            len |= (consume(data) << 4);
            len |= (consume(data) << 12);
            break;
        default:
            dbgout()<<"TODO: Handle " << (b0>>6) << " extra PkgLength bytes\n";
            REQUIRE(false);
    }
    return parse_result<uint32_t>{data, len};
}

array_view<uint8_t> adjust_with_pkg_length(array_view<uint8_t> data)
{
    const uint8_t pkg_length_bytes = 1+(peek(data)>>6);
    auto pkg_len = parse_pkg_length(data);
    REQUIRE(pkg_len.result >= pkg_length_bytes);
    return with_size(pkg_len.data, pkg_len.result-pkg_length_bytes);
}

constexpr bool is_lead_name_char(char c) { return c == '_' || (c >= 'A' && c <= 'Z') ;}
constexpr bool is_name_char(char c) { return is_lead_name_char(c) || (c >= '0' && c <= '9') ;}

constexpr bool valid_nameseg(const char* d) {
    return is_lead_name_char(d[0]) && is_name_char(d[1]) && is_name_char(d[2]) && is_name_char(d[3]);
}
constexpr bool valid_nameseg(const uint8_t* d) {
    return valid_nameseg(reinterpret_cast<const char*>(d));
}

constexpr char root_char             = '\\';
constexpr char parent_prefix_char    = '^';
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

auto parse_name_string(array_view<uint8_t> data)
{
    // NameString := <RootChar NamePath> | <PrefixPath NamePath>

    kstring name;
    if (peek(data) == root_char) {
        name.push_back(consume(data));
    } else {
        while (peek(data) == parent_prefix_char) {
            name.push_back(consume(data));
        }
    }

    // NamePath := NameSeg | DualNamePath | MultiNamePath | NullName
    const auto type = consume(data);
    if (type == 0) { // NullName
        // nothing to do
    } else if (type == dual_name_path_start) {
        // 0x2E -> DualNamePath
        REQUIRE(valid_nameseg(data.begin()));
        REQUIRE(valid_nameseg(data.begin()+4));
        name.insert(name.end(), data.begin(), data.begin() + 8);
        data = advanced(data, 8);
    } else if (type == multi_name_path_start) {
        // MultiNamePath := MultiNamePrefix SegCount NameSeg(SegCount)
        // SegCount := ByteData
        const uint8_t seg_count = consume(data);
        REQUIRE(data.size() >= seg_count*4);
        for (int i = 0; i < seg_count; ++i) {
            REQUIRE(valid_nameseg(data.begin()));
            name.insert(name.end(), data.begin(), data.begin() + 4);
            data = advanced(data, 4);
        }
    } else if (is_lead_name_char(type)) {
        name.push_back(type);
        name.push_back(consume(data));
        name.push_back(consume(data));
        name.push_back(consume(data));
        REQUIRE(valid_nameseg(name.end()-4));
    } else {
        dbgout() << "type = 0x" << as_hex(type) << "\n";
        REQUIRE(false);
    }
    name.push_back('\0');
    return parse_result<kstring>{data, std::move(name)};
}

parse_result<kstring> parse_arg_or_local(array_view<uint8_t> data)
{
    const auto c = consume(data);
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

parse_result<kstring> parse_super_name(array_view<uint8_t> data)
{
    // SimpleName := NameString | ArgObj | LocalObj
    // SuperName := SimpleName | DebugObj | Type6Opcode
    const uint8_t first = peek(data);
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

parse_result<term_list_node_ptr> parse_term_list(array_view<uint8_t> data);
parse_result<node_ptr> parse_data_ref_object(array_view<uint8_t> data);
parse_result<node_ptr> parse_term_arg(array_view<uint8_t> data);

constexpr bool is_data_object(opcode op) {
    return op == opcode::zero
        || op == opcode::one
        || op == opcode::ones
        || op == opcode::byte
        || op == opcode::word
        || op == opcode::dword
        || op == opcode::qword
        || op == opcode::buffer
        || op == opcode::package;
}

parse_result<node_ptr> parse_data_object(array_view<uint8_t> data)
{
    // DataObject := ComputationalData | DefPackage | DefVarPackage
    // ComputationalData := ByteConst | WordConst | DWordConst | QWordConst | String | ConstObj | RevisionOp | DefBuffer
    // ConstObj := ZeroOp | OneOp | OnesOp
    const auto op = static_cast<opcode>(consume(data));
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
                uint8_t b = consume(data);
                return make_parse_result(data, make_const_node(b));
            }
        // WordConst
        case opcode::word:
            {
                uint16_t w = consume(data);
                w |= consume(data) << 8;
                return make_parse_result(data, make_const_node(w));
            }
        // DwordConst
        case opcode::dword:
            {
                uint32_t d = consume(data);
                d |= static_cast<uint32_t>(consume(data)) << 8;
                d |= static_cast<uint32_t>(consume(data)) << 16;
                d |= static_cast<uint32_t>(consume(data)) << 24;
                return make_parse_result(data, make_const_node(d));
            }
        case opcode::qword:
            {
                uint64_t q = consume(data);
                q |= static_cast<uint64_t>(consume(data)) << 8;
                q |= static_cast<uint64_t>(consume(data)) << 16;
                q |= static_cast<uint64_t>(consume(data)) << 24;
                q |= static_cast<uint64_t>(consume(data)) << 32;
                q |= static_cast<uint64_t>(consume(data)) << 40;
                q |= static_cast<uint64_t>(consume(data)) << 48;
                q |= static_cast<uint64_t>(consume(data)) << 56;
                return make_parse_result(data, make_const_node(q));
            }
        case opcode::buffer:
            {
                // DefBuffer := BufferOp PkgLength BufferSize ByteList
                // BufferSize := TermArg => Integer
                auto pkg_data          = adjust_with_pkg_length(data);
                auto buffer_size = parse_term_arg(pkg_data);
                //dbgout() << "Buffer size " << *buffer_size.result << " data:\n";
                //hexdump(dbgout(), buffer_size.data.begin(), buffer_size.data.size());
                return make_parse_result(array_view<uint8_t>(pkg_data.end(), data.end()), knew<buffer_node>(std::move(buffer_size.result), kvector<uint8_t>(buffer_size.data.begin(), buffer_size.data.end())));
            }
        case opcode::package:
            {
                //DefPackage := PackageOp PkgLength NumElements PackageElementList
                //NumElements := ByteData
                //PackageElementList := Nothing | <PackageElement PackageElementList>
                //PackageElement := DataRefObject | NameString
                auto pkg_data = adjust_with_pkg_length(data);
                const uint8_t num_elements = consume(pkg_data);
                kvector<node_ptr> elements;
                while (pkg_data.begin() != pkg_data.end()) {
                    if (is_name_string_start(peek(pkg_data))) {
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
                return make_parse_result(array_view<uint8_t>(pkg_data.begin(), data.end()), knew<package_node>(std::move(elements)));
            }
        default:
            hexdump(dbgout(), data.begin()-2, std::min(data.size()+2, 64ULL));
            dbgout() << "Unhandled opcode 0x" << as_hex(static_cast<uint8_t>(op)) << "\n";
            REQUIRE(false);
    }
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
        || op == opcode::lless;
}

constexpr bool is_type2_ext_opcode(uint8_t ext) {
    return ext == 0x12;
}

parse_result<kstring> parse_target(array_view<uint8_t> data) {
    // Target := SuperName | NullName
    if (peek(data) == 0) {
        consume(data);
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

parse_result<node_ptr> parse_unary_op(array_view<uint8_t> data, const char* name) {
    auto arg    = parse_term_arg(data);
    auto target = parse_target(arg.data);
    //dbgout() << name << " " << *arg.result << " -> " << target.result.begin() << "\n";
    return make_parse_result(target.data, knew<op_node>(name, std::move(arg.result), std::move(target.result)));
}

parse_result<node_ptr> parse_unary_op_no_target(array_view<uint8_t> data, const char* name) {
    auto arg = parse_super_name(data);
    //dbgout() << name << " " << arg.result.begin() << "\n";
    return make_parse_result(arg.data, knew<op_node>(name, make_text_node(arg.result)));
}

parse_result<node_ptr> parse_binary_op(array_view<uint8_t> data, const char* name) {
    // DefAnd := AndOp Operand Operand Target
    // Operand := TermArg => Integer
    // Target := SuperName | NullName
    auto arg0   = parse_term_arg(data);
    auto arg1   = parse_term_arg(arg0.data);
    auto target = parse_target(arg1.data);
    //dbgout() << name << " " << *arg0.result << ", " << *arg1.result << " -> " << target.result.begin() << "\n";
    return make_parse_result(target.data, knew<op_node>(name, std::move(arg0.result), std::move(arg1.result), std::move(target.result)));
}

parse_result<node_ptr> parse_logical_op(array_view<uint8_t> data, const char* name) {
    auto arg0   = parse_term_arg(data);
    auto arg1   = parse_term_arg(arg0.data);
    //dbgout() << name << " " << *arg0.result << ", " << *arg1.result << "\n";
    return make_parse_result(arg1.data, knew<op_node>(name, std::move(arg0.result), std::move(arg1.result)));
}

parse_result<node_ptr> parse_type2_opcode(array_view<uint8_t> data) {
    const auto op = static_cast<opcode>(consume(data));
    if (op == opcode::ext_prefix) {
        if (!is_type2_ext_opcode(peek(data))) {
            dbgout() << "op = 0x5B 0x" << as_hex(peek(data)) << "\n";
            REQUIRE(false);
        }
    } else {
        if (!is_type2_opcode(op)) {
            dbgout() << "op = 0x" << as_hex(static_cast<uint8_t>(op)) << "\n";
            REQUIRE(false);
        }
    }
    switch (op) {
        case opcode::ext_prefix:
            {
                const auto ext_op = consume(data);
                if (ext_op == 0x12) {
                    // DefCondRefOf := CondRefOfOp SuperName Target
                    auto name   = parse_super_name(data);
                    auto target = parse_target(name.data);
                    //dbgout() << "CondRefOf " << name.result.begin() << " -> " << target.result.begin() << "\n";
                    return make_parse_result(target.data, knew<op_node>("cond_ref_of", make_text_node(name.result), std::move(target.result)));
                } else {
                    dbgout() << "Unhadled extended op " << as_hex(ext_op) << "\n";
                    REQUIRE(false);
                }
            }
            break;
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
                const auto next = static_cast<opcode>(peek(data));
                if (next == opcode::lequal) {
                    consume(data);
                    return parse_logical_op(data, "lnotequal");
                } else if (next == opcode::lgreater) {
                    consume(data);
                    return parse_logical_op(data, "lnotgreater");
                } else if (next == opcode::lless) {
                    consume(data);
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
        default:
            hexdump(dbgout(), data.begin()-2, std::min(data.size()+2, 64ULL));
            dbgout() << "Unhandled opcode 0x" << as_hex(static_cast<uint8_t>(op)) << "\n";
            REQUIRE(false);
    }

    REQUIRE(false);
}

parse_result<node_ptr> parse_term_arg(array_view<uint8_t> data) {
    // TermArg := Type2Opcode | DataObject | ArgObj | LocalObj
    const uint8_t first = peek(data);
    //dbgout() << "parse_term_arg first = " << as_hex(first) << "\n";
    if (is_arg_or_local(first)) {
        auto name = parse_arg_or_local(data);
        return make_parse_result(name.data, make_text_node(name.result));
    } else if (is_name_string_start(first)) {
        // It seems NameSeg is valid here. E.g. ReturnOp 'TMP_'
        // Is this sometimes a method invocation?
        auto name = parse_name_string(data);
        //dbgout() << name.result.begin() << "\n";
        //hexdump(dbgout(), name.data.begin(), name.data.size());
        return make_parse_result(name.data, make_text_node(name.result));
    } else if (is_data_object(static_cast<opcode>(first))) {
        return parse_data_object(data);
    } else {
        // Type2Opcode
        return parse_type2_opcode(data);
    }
}


parse_result<uint8_t> parse_field_flags(array_view<uint8_t> data) {
    const uint8_t flags = consume(data);
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

parse_result<node_ptr> parse_named_field(array_view<uint8_t> data)
{
    REQUIRE(data.size() >= 4);
    REQUIRE(valid_nameseg(reinterpret_cast<const char*>(data.begin())));
    kstring name;
    name.push_back(consume(data));
    name.push_back(consume(data));
    name.push_back(consume(data));
    name.push_back(consume(data));
    name.push_back('\0');
    const auto len = parse_pkg_length(data);
    //dbgout() << "NamedField " << name.begin() << " PkgLen 0x" <<  as_hex(len.result).width(0) << "\n";
    return make_parse_result(len.data, knew<named_field_node>(std::move(name), len.result));
}

parse_result<field_list_node_ptr> parse_field_list(array_view<uint8_t> data)
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
        const uint8_t first = peek(data);
        if (first == 0) {
            consume(data);
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

parse_result<obj_list_node_ptr> parse_object_list(array_view<uint8_t> data);

class device_node : public node {
public:
    explicit device_node(kstring&& name, obj_list_node_ptr&& objs) : name_(std::move(name)), objs_(std::move(objs)) {
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


parse_result<node_ptr> parse_ext_prefix(array_view<uint8_t> data)
{
    enum class ext_op : uint8_t {
        op_region = 0x80,
        field     = 0x81,
        device    = 0x82,
    };

    const auto ext = static_cast<ext_op>(consume(data));
    switch (ext) {
        case ext_op::op_region:
            {
                // DefOpRegion := OpRegionOp NameString RegionSpace(byte) RegionOffset(term=>int) RegionLen(term=>int)
                auto name = parse_name_string(data);
                data = name.data;
                uint8_t region_space = consume(data);
                auto region_offset   = parse_term_arg(data);
                auto region_len      = parse_term_arg(region_offset.data);
                //dbgout() << "DefOpRegion " << name.result.begin() << " Region space " << as_hex(region_space) << " offset " << *region_offset.result << " len " << *region_len.result << "\n";
                return make_parse_result(region_len.data, knew<op_region_node>(std::move(name.result), region_space, std::move(region_offset.result), std::move(region_len.result)));
            }
        case ext_op::field:
            {
                // DefField := FieldOp PkgLength NameString FieldFlags FieldList
                auto name        = parse_name_string(adjust_with_pkg_length(data));
                auto field_flags = parse_field_flags(name.data);
                //dbgout() << "DefField " << name.result.begin() << " flags " << *field_flags.result << "\n";
                auto field_list  = parse_field_list(field_flags.data);
                return make_parse_result(array_view<uint8_t>(field_list.data.begin(), data.end()), knew<field_node>(std::move(name.result), field_flags.result, std::move(field_list.result)));
            }
        case ext_op::device:
            {
                // DefDevice := DeviceOp PkgLength NameString ObjectList
                auto name        = parse_name_string(adjust_with_pkg_length(data));
                auto object_list = parse_object_list(name.data);
                //dbgout() << "DefDevice " << name.result.begin() << "\n";
                //return make_parse_result(array_view<uint8_t>(object_list.data.begin(), data.end()), knew<dummy_node>("device"));
                return make_parse_result(array_view<uint8_t>(object_list.data.begin(), data.end()), knew<device_node>(std::move(name.result), std::move(object_list.result)));
            }
        default:
            dbgout() << "Unhandled ext opcode 0x5b 0x" << as_hex(static_cast<uint8_t>(ext)) << "\n";
            REQUIRE(false);
    }
}

parse_result<node_ptr> parse_data_ref_object(array_view<uint8_t> data)
{
    // DataRefObject := DataObject | ObjectReference | DDBHandle
    return parse_data_object(data); // TODO: ObjectReference | DDBHandle
}

class name_node : public node {
public:
    explicit name_node(kstring&& name, node_ptr&& obj) : name_(std::move(name)), obj_(std::move(obj)) {
    }

private:
    kstring  name_;
    node_ptr obj_;
    virtual void do_print(out_stream& os) const override {
        os << "Name " << name_.begin() << " " << *obj_;
    }
};


class method_node : public node {
public:
    explicit method_node(kstring&& name, uint8_t flags, term_list_node_ptr&& statements) : name_(std::move(name)), flags_(flags), statements_(std::move(statements)) {
    }

private:
    kstring            name_;
    uint8_t            flags_;
    term_list_node_ptr statements_;
    virtual void do_print(out_stream& os) const override {
        os << "Method " << name_.begin() << " Flags " << as_hex(flags_) << " " << *statements_;
    }
};

class scope_node : public node {
public:
    explicit scope_node(kstring&& name, term_list_node_ptr&& statements) : name_(std::move(name)), statements_(std::move(statements)) {
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

parse_result<node_ptr> parse_term_obj(array_view<uint8_t> data)
{
    // TermObj := NameSpaceModifierObject | NamedObj | Type1Opcode | Type2Opcode
    // NameSpaceModifierObject := DefAlias | DefName | DefScope
    const auto op = static_cast<opcode>(peek(data));
    if (is_type2_opcode(op) || (op == opcode::ext_prefix && is_type2_ext_opcode(data[1]))) {
        return parse_type2_opcode(data);
    }
    consume(data);
    switch (op) {
        case opcode::name:
            {
                // DefName := NameOp NameString DataRefOjbect
                auto name            = parse_name_string(data);
                auto data_ref_object = parse_data_ref_object(name.data);
                //dbgout() << "DefName " << name.result.begin() << " " << *data_ref_object.result << "\n";
                return make_parse_result(data_ref_object.data, knew<name_node>(std::move(name.result), std::move(data_ref_object.result)));
            }
        case opcode::scope:
            {
                // DefScope := ScopeOp PkgLength NameString TermList
                auto name    = parse_name_string(adjust_with_pkg_length(data));
                auto term_list = parse_term_list(name.data);
                //dbgout() << "DefScope " << name.result.begin() << "\n";
                return make_parse_result(array_view<uint8_t>(term_list.data.begin(), data.end()), knew<scope_node>(std::move(name.result), std::move(term_list.result)));
            }
        case opcode::method:
            {
                // DefMethod := MethodOp PkgLength NameString MethodFlags TermList
                auto pkg_data = adjust_with_pkg_length(data);
                auto name = parse_name_string(pkg_data);
                pkg_data = name.data;
                uint8_t method_flags = consume(pkg_data);
                auto term_list = parse_term_list(pkg_data);
                //dbgout() << "DefMethod " << name.result.begin() << " Flags " << as_hex(method_flags) << "\n";
                return make_parse_result(array_view<uint8_t>(term_list.data.begin(), data.end()), knew<method_node>(std::move(name.result), method_flags, std::move(term_list.result)));
                //return make_parse_result(array_view<uint8_t>(term_list.data.begin(), data.end()), knew<dummy_node>("method"));
            }
        case opcode::ext_prefix:
                return parse_ext_prefix(data);
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
                data = array_view<uint8_t>(term_list.data.begin(), data.end());
                // DefElse := Nothing | <ElseOp PkgLength TermList>
                term_list_node_ptr else_statements;
                if (static_cast<opcode>(peek(data)) == opcode::else_) {
                    consume(data);
                    auto else_term_list = parse_term_list(adjust_with_pkg_length(data));
                    data = array_view<uint8_t>(else_term_list.data.begin(), data.end());
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
                return make_parse_result(array_view<uint8_t>(term_list.data.begin(), data.end()), knew<dummy_node>("while"));
            }
        case opcode::return_:
            {
                // DefReturn := ReturnOp ArgObject
                // ArgObject := TermArg => DataRefObject
                auto arg = parse_term_arg(data);
                //dbgout() << "Return " << *arg.result << "\n";
                return make_parse_result(arg.data, knew<op_node>("return", std::move(arg.result)));
            }
        default:
            dbgout() << "Unhandled opcode 0x" << as_hex(static_cast<uint8_t>(op)) << "\n";
            REQUIRE(false);
    }
}

parse_result<obj_list_node_ptr> parse_object_list(array_view<uint8_t> data)
{
    // ObjectList := Nothing | <Object ObjectList>
    // Object := NameSpaceModifierObj | NamedObj
    kvector<node_ptr> objs;
    while (data.begin() != data.end()) {
        auto res = parse_term_obj(data); // TODO: Type1Opcode | Type2Opcode not allowed in ObjectList
        data = res.data;
        objs.push_back(std::move(res.result));
    }
    return {data, knew<obj_list_node>(std::move(objs))};
}

parse_result<term_list_node_ptr> parse_term_list(array_view<uint8_t> data)
{
    // TermList := Nothing | <TermObj TermList>
    kvector<node_ptr> terms;
    while (data.begin() != data.end()) {
        auto res = parse_term_obj(data);
        data = res.data;
        terms.push_back(std::move(res.result));
    }
    return {data, knew<term_list_node>(std::move(terms))};
}

void process(array_view<uint8_t> data)
{
    dbgout() << *parse_term_list(data).result << "\n";
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

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

struct dummy{};

parse_result<dummy> parse_term_list(array_view<uint8_t> data);
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
                const auto buffer_size = parse_term_arg(pkg_data);
                dbgout() << "Buffer size " << *buffer_size.result << " data:\n";
                hexdump(dbgout(), buffer_size.data.begin(), buffer_size.data.size());
                return make_parse_result(array_view<uint8_t>(pkg_data.end(), data.end()), knew<dummy_node>("Buffer"));
            }
        case opcode::package:
            {
                //DefPackage := PackageOp PkgLength NumElements PackageElementList
                //NumElements := ByteData
                //PackageElementList := Nothing | <PackageElement PackageElementList>
                //PackageElement := DataRefObject | NameString
                auto pkg_data = adjust_with_pkg_length(data);
                const uint8_t num_elements = consume(pkg_data);
                dbgout() << "NumElements = " << (int)num_elements << "\n";
                while (pkg_data.begin() != pkg_data.end()) {
                    if (is_name_string_start(peek(pkg_data))) {
                        auto name = parse_name_string(pkg_data);
                        dbgout() << "PackageElement NameString " << name.result.begin() << "\n";
                        pkg_data = name.data;
                    } else {
                        auto data_ref_object = parse_data_ref_object(pkg_data);
                        dbgout() << "PackageElement DataRefObject " << *data_ref_object.result << "\n";
                        pkg_data = data_ref_object.data;
                    }
                }
                dbgout() << "\n";
                return make_parse_result(array_view<uint8_t>(pkg_data.begin(), data.end()), knew<dummy_node>("Package"));
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

parse_result<node_ptr> parse_unary_op(array_view<uint8_t> data, const char* name) {
    const auto arg    = parse_term_arg(data);
    const auto target = parse_target(arg.data);
    dbgout() << name << " " << *arg.result << " -> " << target.result.begin() << "\n";
    return make_parse_result(target.data, knew<dummy_node>(name));
}

parse_result<node_ptr> parse_unary_op_no_target(array_view<uint8_t> data, const char* name) {
    const auto arg = parse_super_name(data);
    dbgout() << name << " " << arg.result.begin() << "\n";
    return make_parse_result(arg.data, knew<dummy_node>(name));
}

parse_result<node_ptr> parse_binary_op(array_view<uint8_t> data, const char* name) {
    // DefAnd := AndOp Operand Operand Target
    // Operand := TermArg => Integer
    // Target := SuperName | NullName
    const auto arg0   = parse_term_arg(data);
    const auto arg1   = parse_term_arg(arg0.data);
    const auto target = parse_target(arg1.data);
    dbgout() << name << " " << *arg0.result << ", " << *arg1.result << " -> " << target.result.begin() << "\n";
    return make_parse_result(target.data, knew<dummy_node>(name));
}

parse_result<node_ptr> parse_logical_op(array_view<uint8_t> data, const char* name) {
    const auto arg0   = parse_term_arg(data);
    const auto arg1   = parse_term_arg(arg0.data);
    dbgout() << name << " " << *arg0.result << ", " << *arg1.result << "\n";
    return make_parse_result(arg1.data, knew<dummy_node>(name));
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
                    const auto name   = parse_super_name(data);
                    const auto target = parse_target(name.data);
                    dbgout() << "CondRefOf " << name.result.begin() << " -> " << target.result.begin() << "\n";
                    return make_parse_result(target.data, knew<dummy_node>("cond_ref_of"));
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
                const auto arg = parse_term_arg(data);
                // DefDerefOf := DerefOfOp ObjReference
                // ObjReference := TermArg => ObjectReference | String
                dbgout() << "deref_of " << *arg.result << "\n";
                return make_parse_result(arg.data, knew<dummy_node>("deref_of"));
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
        return make_parse_result(name.data, knew<dummy_node>(name.result.begin()));
    } else if (is_name_string_start(first)) {
        // It seems NameSeg is valid here. E.g. ReturnOp 'TMP_'
        // Is this sometimes a method invocation?
        auto name = parse_name_string(data);
        dbgout() << name.result.begin() << "\n";
        hexdump(dbgout(), name.data.begin(), name.data.size());
        return make_parse_result(name.data, knew<dummy_node>(name.result.begin()));
    } else if (is_data_object(static_cast<opcode>(first))) {
        return parse_data_object(data);
    } else {
        // Type2Opcode
        return parse_type2_opcode(data);
    }
}


auto parse_field_flags(array_view<uint8_t> data) {
    const uint8_t flags = consume(data);
    return make_parse_result(data, make_const_node(flags));
}

parse_result<kstring> parse_named_field(array_view<uint8_t> data)
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
    dbgout() << "NamedField " << name.begin() << " PkgLen 0x" <<  as_hex(len.result).width(0) << "\n";
    return parse_result<kstring>{len.data, name};
}

parse_result<dummy> parse_field_list(array_view<uint8_t> data)
{
    // FieldList := Nothing | <FieldElement FieldList>
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
            dbgout() << "ReservedField " << " PkgLen 0x" <<  as_hex(len.result).width(0) << "\n";
            data = len.data;
        } else if (is_lead_name_char(first)) {
            const auto named_field = parse_named_field(data);
            data = named_field.data;
        } else {
            dbgout() << "Unknown FieldElement " << as_hex(first) << "\n";
            REQUIRE(false);
        }
    }
    return parse_result<dummy>{data, dummy{}};
}

parse_result<dummy> parse_object_list(array_view<uint8_t> data);

parse_result<dummy> parse_ext_prefix(array_view<uint8_t> data)
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
                const auto name = parse_name_string(data);
                data = name.data;
                const uint8_t region_space = consume(data);
                const auto region_offset   = parse_term_arg(data);
                const auto region_len      = parse_term_arg(region_offset.data);
                dbgout() << "DefOpRegion " << name.result.begin() << " Region space " << as_hex(region_space);
                dbgout() << " offset " << *region_offset.result << " len " << *region_len.result;
                dbgout() << "\n";
                return parse_result<dummy>{region_len.data, dummy{}};
            }
        case ext_op::field:
            {
                // DefField := FieldOp PkgLength NameString FieldFlags FieldList
                const auto name        = parse_name_string(adjust_with_pkg_length(data));
                const auto field_flags = parse_field_flags(name.data);
                dbgout() << "DefField " << name.result.begin() << " flags " << *field_flags.result << "\n";
                const auto field_list  = parse_field_list(field_flags.data);
                return parse_result<dummy>{array_view<uint8_t>(field_list.data.begin(), data.end()), dummy{}};
            }
        case ext_op::device:
            {
                // DefDevice := DeviceOp PkgLength NameString ObjectList
                const auto name        = parse_name_string(adjust_with_pkg_length(data));
                dbgout() << "DefDevice " << name.result.begin() << "\n";
                const auto object_list = parse_object_list(name.data);
                return parse_result<dummy>{array_view<uint8_t>(object_list.data.begin(), data.end()), dummy{}};
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
                const auto name            = parse_name_string(data);
                dbgout() << "DefName " << name.result.begin() << "\n";// << " " << *data_ref_object.result << "\n";
                const auto data_ref_object = parse_data_ref_object(name.data);
                return make_parse_result(data_ref_object.data, knew<dummy_node>("name"));
            }
        case opcode::scope:
            {
                // DefScope := ScopeOp PkgLength NameString TermList
                const auto name    = parse_name_string(adjust_with_pkg_length(data));
                dbgout() << "DefScope " << name.result.begin() << "\n";
                const auto term_list = parse_term_list(name.data);
                return make_parse_result(array_view<uint8_t>(term_list.data.begin(), data.end()), knew<dummy_node>("scope"));
            }
        case opcode::method:
            {
                // DefMethod := MethodOp PkgLength NameString MethodFlags TermList
                auto pkg_data = adjust_with_pkg_length(data);
                const auto name = parse_name_string(pkg_data);
                pkg_data = name.data;
                const uint8_t method_flags = consume(pkg_data);
                dbgout() << "DefMethod " << name.result.begin() << " Flags " << as_hex(method_flags) << "\n";
                const auto term_list = parse_term_list(pkg_data);
                return make_parse_result(array_view<uint8_t>(term_list.data.begin(), data.end()), knew<dummy_node>("method"));
            }
        case opcode::ext_prefix:
            {
                const auto res = parse_ext_prefix(data);
                return make_parse_result(res.data, knew<dummy_node>("extprefix"));
            }
        case opcode::store:
            {
                // DefStore := StoreOp TermArg SuperName
                const auto arg = parse_term_arg(data);
                const auto name = parse_super_name(arg.data);
                dbgout() << "Store " << *arg.result << " -> " << name.result.begin() << "\n";
                return make_parse_result(name.data, knew<dummy_node>("store"));
            }
        case opcode::create_word_field:
            {
                // DefCreateWordField := CreateWordFieldOp SourceBuff ByteIndex NameString
                // SourceBuff := TermArg => Buffer
                // ByteIndex := TermArg => Integer
                const auto source_buffer = parse_term_arg(data);
                const auto byte_index    = parse_term_arg(source_buffer.data);
                const auto name          = parse_name_string(byte_index.data);
                dbgout() << "DefCreateWordField " << *source_buffer.result << " Index " << *byte_index.result << " Name " << name.result.begin() << "\n";
                return make_parse_result(name.data, knew<dummy_node>("create_word_field"));
            }
            break;
        case opcode::if_:
            {
                // DefIfElse := IfOp PkgLength Predicate TermList DefElse
                // Predicate := TermArg => Integer
                const auto predicate = parse_term_arg(adjust_with_pkg_length(data));
                dbgout() << "DefIfElse predicate = " << *predicate.result << "\n";
                const auto term_list = parse_term_list(predicate.data);
                data = array_view<uint8_t>(term_list.data.begin(), data.end());
                // DefElse := Nothing | <ElseOp PkgLength TermList>
                if (static_cast<opcode>(peek(data)) == opcode::else_) {
                    consume(data);
                    const auto else_term_list = parse_term_list(adjust_with_pkg_length(data));
                    data = array_view<uint8_t>(else_term_list.data.begin(), data.end());
                }
                return make_parse_result(data, knew<dummy_node>("if"));
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
                const auto arg = parse_term_arg(data);
                dbgout() << "Return " << *arg.result << "\n";
                return make_parse_result(arg.data, knew<dummy_node>("return"));
            }
        default:
            dbgout() << "Unhandled opcode 0x" << as_hex(static_cast<uint8_t>(op)) << "\n";
            REQUIRE(false);
    }
}

parse_result<dummy> parse_object_list(array_view<uint8_t> data)
{
    // ObjectList := Nothing | <Object ObjectList>
    // Object := NameSpaceModifierObj | NamedObj
    while (data.begin() != data.end()) {
        const auto res = parse_term_obj(data); // TODO: Type1Opcode | Type2Opcode not allowed in ObjectList
        data = res.data;
    }
    return {data};
}

parse_result<dummy> parse_term_list(array_view<uint8_t> data)
{
    // TermList := Nothing | <TermObj TermList>
    while (data.begin() != data.end()) {
        const auto res = parse_term_obj(data);
        data = res.data;
    }
    return {data};
}

void process(array_view<uint8_t> data)
{
    parse_term_list(data);
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

int main()
{
    kvector<uint8_t> data;
    //const char* filename = "vmware_dsdt.aml";
    const char* filename = "bochs_dsdt.aml";
    if (!read_file(filename, data)) {
        dbgout() << "Could not read " << filename << "\n";
        return 1;
    }
    aml::process(make_array_view(data.begin(), data.size()));
}

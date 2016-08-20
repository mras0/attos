#include <attos/out_stream.h>
#include <attos/cpu.h>
#include <attos/containers.h>
#include <attos/array_view.h>

namespace attos { namespace aml {

enum class opcode : uint8_t {
    name       = 0x08,
    byte       = 0x0a, // BytedData
    word       = 0x0b, // WordData
    scope      = 0x10,
    ext_prefix = 0x5b, // '[' ByteData follows specifying the extended opcode
};

template<typename T>
struct parse_result {
    array_view<uint8_t> data;
    T                   result;
};

template<typename T>
parse_result<T> make_parse_result(array_view<uint8_t> data, T&& t) {
    return parse_result<T>{data, static_cast<T&&>(t)};
}

char peek(array_view<uint8_t> data) {
    return *data.begin();
}

uint8_t consume(array_view<uint8_t>& data) {
    const auto b = *data.begin();
    data = array_view<uint8_t>(data.begin() + 1, data.end());
    return b;
}

array_view<uint8_t> with_size(array_view<uint8_t> data, uint32_t size) {
    REQUIRE(data.size() >= size);
    return array_view<uint8_t>(data.begin(), data.begin() + size);
}

auto parse_pkg_length(array_view<uint8_t> data)
{
    const uint8_t b0 = consume(data);
    REQUIRE(b0 < 63); // Single byte pkg length
    // bit 7-6 bytes that follow (0-3)
    // bit 5-4 only used if PkgLength < 63
    // bit 3-0 least significant packet length nibble
    return parse_result<uint32_t>{data, b0};
}

constexpr bool is_lead_name_char(char c) { return c == '_' || (c >= 'A' && c <= 'Z') ;}
constexpr bool is_name_char(char c) { return is_lead_name_char(c) || (c >= '0' && c <= '9') ;}

auto parse_name_string(array_view<uint8_t> data)
{
    // NameString := <RootChar NamePath> | <PrefixPath NamePath>
    constexpr char root_char          = '\\';
    constexpr char parent_prefix_char = '^';

    kvector<char> name;
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
    } else if (is_lead_name_char(type)) {
        name.push_back(type);
        REQUIRE(is_name_char(peek(data)));
        name.push_back(consume(data));
        REQUIRE(is_name_char(peek(data)));
        name.push_back(consume(data));
        REQUIRE(is_name_char(peek(data)));
        name.push_back(consume(data));
    } else {
        // 0x2E -> DualNamePath, 0x2F -> MultiNamePath
        dbgout() << "type = 0x" << as_hex(type) << "\n";
        REQUIRE(false);
    }
    name.push_back('\0');
    return make_parse_result(data, name);
}

struct dummy{};

parse_result<dummy> parse_term_list(array_view<uint8_t> data);

parse_result<uint32_t> parse_term_arg(array_view<uint8_t> data)
{
    const auto op = static_cast<opcode>(consume(data));
    switch (op) {
        case opcode::byte:
            {
                uint8_t b = consume(data);
                return parse_result<uint32_t>{data, b};
            }
        case opcode::word:
            {
                uint16_t w = consume(data);
                w |= consume(data) << 8;
                return parse_result<uint32_t>{data, w};
            }
        default:
            dbgout() << "Unhandled opcode 0x" << as_hex(static_cast<uint8_t>(op)) << "\n";
            REQUIRE(false);
    }
}

parse_result<dummy> parse_ext_prefix(array_view<uint8_t> data)
{
    enum class ext_op : uint8_t {
        op_region = 0x80,
        field     = 0x81,
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
                dbgout() << " offset " << as_hex(region_offset.result) << " len " << as_hex(region_len.result);
                dbgout() << "\n";
                return parse_result<dummy>{region_len.data, dummy{}};
            }
        case ext_op::field:
            {
                // DefField := FieldOp PkgLength NameString FieldFlags FieldList
                const auto pkg_len = parse_pkg_length(data);
                const auto name    = parse_name_string(with_size(pkg_len.data, pkg_len.result));
                hexdump(dbgout(), data.begin(), 32);
                REQUIRE(false);
            }
        default:
            dbgout() << "Unhandled ext opcode 0x5b 0x" << as_hex(static_cast<uint8_t>(ext)) << "\n";
            REQUIRE(false);
    }
}

parse_result<dummy> parse_term_obj(array_view<uint8_t> data)
{
    // TermObj := NameSpaceModifierObject | NamedObj | Type1Opcode | Type2Opcode
    // NameSpaceModifierObject := DefAlias | DefName | DefScope
    const auto op = static_cast<opcode>(consume(data));
    switch (op) {
        case opcode::name:
            {
                // DefName := NameOp NameString DataRefOjbect
                const auto name = parse_name_string(data);
                dbgout() << "DefName " << name.result.begin() << "\n";
                REQUIRE(false);
                break;
            }
        case opcode::scope:
            {
                // DefScope := ScopeOp PkgLength NameString TermList
                const auto pkg_len = parse_pkg_length(data);
                const auto name    = parse_name_string(with_size(pkg_len.data, pkg_len.result));
                dbgout() << "DefScope " << name.result.begin() << "\n";
                parse_term_list(name.data);
                REQUIRE(false); // data.advance(pkg_len.result)... or something
            }
        case opcode::ext_prefix:
            {
                const auto res = parse_ext_prefix(data);
                return parse_result<dummy>{res.data, dummy{}};
            }
        default:
            dbgout() << "Unhandled opcode 0x" << as_hex(static_cast<uint8_t>(op)) << "\n";
            REQUIRE(false);
    }
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
    const char* filename = "bochs_dsdt.aml";
    if (!read_file(filename, data)) {
        dbgout() << "Could not read " << filename << "\n";
        return 1;
    }
    aml::process(make_array_view(data.begin(), data.size()));
}

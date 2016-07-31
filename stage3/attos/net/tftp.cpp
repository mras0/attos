#include "tftp.h"
#include <attos/string.h>
#include <attos/cpu.h>

namespace attos { namespace net { namespace tftp {

uint8_t* put(uint8_t* b, const char* s) {
    const auto l = string_length(s);
    memcpy(b, s, l + 1); // also copy nul-terminator
    return b + l + 1;
}

uint8_t* put_error_reply(uint8_t* b, error_code ec, const char* msg) {
    b = put(b, opcode::error);
    b = put(b, static_cast<uint16_t>(ec));
    return put(b, msg);
}

uint16_t get_u16(const uint8_t*& data, uint32_t& length) {
    REQUIRE(length >= 2);
    const auto x = static_cast<uint16_t>(data[0]*256 + data[1]);
    data   += 2;
    length -= 2;
    return x;
}

opcode get_opcode(const uint8_t*& data, uint32_t& length) {
    return static_cast<opcode>(get_u16(data, length));
}

const char* get_string(const uint8_t*& data, uint32_t& length) {
    auto str = reinterpret_cast<const char*>(data);
    while (length--) {
        if (!*data++) {
            return str;
        }
    }
    REQUIRE(!"Invalid TFTP string");
    return nullptr;
}

} } } // namespace attos::net::tftp


#include <attos/net/net.h>
#include <attos/out_stream.h>

namespace attos { namespace net {

mac_address mac_address::broadcast{0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

bool operator==(const mac_address& l, const mac_address& r) {
    return std::equal(&l[0], &l[6], &r[0]);
}

out_stream& operator<<(out_stream& os, const mac_address& mac) {
    for (int i = 0; i < 6; ++i) {
        if (i) os << ':';
        os << as_hex(mac[i]);
    }
    return os;
}

out_stream& operator<<(out_stream& os, const ipv4_address& ip) {
    const uint32_t i = ip.host_u32();
    return os << ((i>>24)&255) << '.' << ((i>>16)&255) << '.' << ((i>>8)&255) << '.' << (i&255);
}

ethernet_device::~ethernet_device() {
}

uint16_t inet_csum(const void * src, uint16_t length, uint16_t init) {
    auto buf = static_cast<const uint8_t*>(src);
    uint32_t result = init;
    while (length > 1) {
        result += bswap(*reinterpret_cast<const uint16_t*>(buf));
        length -= 2;
        buf    += 2;
    }
    if (length) result += *buf << 8;
    result = (result >> 16) + (result & 0xFFFF);
    result += (result >> 16);
    result = (~result)&0xFFFF;
    return static_cast<uint16_t>(result);
}

} } // namespace attos::net

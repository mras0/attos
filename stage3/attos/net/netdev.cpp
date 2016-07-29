#include <attos/net/netdev.h>
#include <attos/out_stream.h>

namespace attos { namespace net {

mac_address mac_address::broadcast{0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

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

netdev::~netdev() {
}

} } // namespace attos::net

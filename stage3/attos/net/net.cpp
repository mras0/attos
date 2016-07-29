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

} } // namespace attos::net

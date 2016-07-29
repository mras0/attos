#ifndef ATTOS_NET_NETDEV_H
#define ATTOS_NET_NETDEV_H

#include <attos/mm.h>
#include <attos/function.h>
#include <array>

namespace attos {
class out_stream;
} // namespace attos

namespace attos { namespace net {

class mac_address {
public:
    constexpr mac_address() = default;

    explicit constexpr mac_address(const uint8_t* b) : b_({b[0], b[1], b[2], b[3], b[4], b[5]}) {
    }

    explicit constexpr mac_address(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5) : b_{b0, b1, b2, b3, b4, b5} {
    }

    constexpr const uint8_t& operator[](size_t index) const { return b_[index]; }

    static mac_address broadcast;

private:
    std::array<uint8_t, 6> b_;
};
static_assert(sizeof(mac_address) == 6, "");

out_stream& operator<<(out_stream& os, const mac_address& mac);

class ipv4_address {
public:
    constexpr ipv4_address() : ip_(0) {
    }

    explicit constexpr ipv4_address(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : ip_((a<<24)|(b<<16)|(c<<8)|d) {
    }

    constexpr uint32_t host_u32() const { return ip_; }

private:
    uint32_t ip_;
};
static_assert(sizeof(ipv4_address) == 4, "");
out_stream& operator<<(out_stream& os, const ipv4_address& ip);

using packet_process_function = function<void (const uint8_t*, uint32_t)>;

class __declspec(novtable) netdev {
public:
    virtual ~netdev();

    mac_address hw_address() const {
        return do_hw_address();
    }

    void send_packet(const void* data, uint32_t length) {
        do_send_packet(data, length);
    }

    void process_packets(const packet_process_function& ppf) {
        do_process_packets(ppf);
    }

private:
    virtual mac_address do_hw_address() const = 0;
    virtual void do_send_packet(const void* data, uint32_t length) = 0;
    virtual void do_process_packets(const packet_process_function& ppf) = 0;
};

using netdev_ptr = kowned_ptr<netdev>;

} } // namespace attos::net

#endif

#ifndef ATTOS_NET_NET_H
#define ATTOS_NET_NET_H

#include <attos/mm.h>
#include <attos/function.h>
#include <array>
#include <type_traits>

namespace attos {
class out_stream;
} // namespace attos

namespace attos { namespace net {

constexpr inline uint16_t bswap(uint16_t x) { return (x<<8) | (x>>8); }
constexpr inline uint32_t bswap(uint32_t x) { return (x<<24) | ((x<<8) & 0xff0000) | ((x>>8) & 0xff00) | (x>>24); }

template<typename T, typename U = typename std::enable_if<std::is_enum<T>::value>::type>
constexpr inline T bswap(T x) {
    return static_cast<T>(bswap(static_cast<std::underlying_type_t<T>>(x)));
}

static_assert(bswap(static_cast<uint16_t>(0x0608)) == 0x0806, "");
static_assert(bswap(static_cast<uint32_t>(0x01020304)) == 0x04030201, "");

template<typename T>
class be_int {
public:
    constexpr be_int() = default;
    constexpr be_int(const be_int& rhs) : val_(rhs.val_) {}
    constexpr be_int(T x) : val_(bswap(x)) {}
    be_int& operator=(be_int rhs) {
        val_ = rhs.val_;
        return *this;
    }
    be_int& operator=(T x) {
        val_ = bswap(x);
        return *this;
    }
    constexpr operator T() const { return bswap(val_); }

    template<typename R = typename std::enable_if<std::is_enum<T>::value>::type>
    explicit operator R() const {
        return bswap(static_cast<std::underlying_type_t<T>>(val_));
    }
private:
    T val_;
};
using le_uint16_t = uint16_t;
using le_uint32_t = uint32_t;
using be_uint16_t = be_int<uint16_t>;
using be_uint32_t = be_int<uint32_t>;
static_assert(sizeof(be_uint16_t)==2,"");
static_assert(sizeof(be_uint32_t)==4,"");
static_assert(be_uint16_t(0x0102) == 0x0102, "");

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
bool operator==(const mac_address& l, const mac_address& r);
inline bool operator!=(const mac_address& l, const mac_address& r) {
    return !(l == r);
}
out_stream& operator<<(out_stream& os, const mac_address& mac);

class ipv4_address {
public:
    constexpr ipv4_address() : ip_(0) {
    }

    explicit constexpr ipv4_address(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : ip_((a<<24)|(b<<16)|(c<<8)|d) {
    }

    constexpr uint32_t host_u32() const { return ip_; }

private:
    be_uint32_t ip_;
};
static constexpr ipv4_address inaddr_any       = ipv4_address{  0,   0,   0,   0};
static constexpr ipv4_address inaddr_broadcast = ipv4_address{255, 255, 255, 255};

constexpr inline bool operator==(ipv4_address l, ipv4_address r) { return l.host_u32() == r.host_u32(); }
constexpr inline bool operator!=(ipv4_address l, ipv4_address r) { return !(l == r); }

static_assert(sizeof(ipv4_address) == 4, "");
out_stream& operator<<(out_stream& os, const ipv4_address& ip);

using packet_process_function = function<void (const uint8_t*, uint32_t)>;

class __declspec(novtable) ethernet_device {
public:
    virtual ~ethernet_device();

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

using ethernet_device_ptr = kowned_ptr<ethernet_device>;

enum class ethertype : uint16_t {
    ipv4 = 0x0800,
    arp  = 0x0806,
};

#pragma pack(push, 1)
struct ethernet_header {
    mac_address       dst;
    mac_address       src;
    be_int<ethertype> type;
};
static_assert(sizeof(ethernet_header) == 14, "");

enum class arp_htype : uint16_t {
    ethernet = 0x0001,
};

enum class arp_operation : uint16_t {
    request = 1,
    reply   = 2,
};

struct arp_header {
    be_int<arp_htype>     htype; // Hardware type
    be_int<ethertype>     ptype; // Protocol type
    uint8_t               hlen;  // Hardware address length
    uint8_t               plen;  // Protocol address length
    be_int<arp_operation> oper;  // Operation
    mac_address           sha;   // Sender hardware address
    ipv4_address          spa;   // Sender protocol address
    mac_address           tha;   // Target hardware address
    ipv4_address          tpa;   // Target protocol address
};
static_assert(sizeof(arp_header) == 28, "");
#pragma pack(pop)

} } // namespace attos::net

#endif

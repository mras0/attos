#ifndef ATTOS_NET_TFTP_H
#define ATTOS_NET_TFTP_H

#include <attos/net/net.h>
#include <attos/containers.h>
#include <attos/array_view.h>
#include <attos/function.h>

namespace attos { namespace net {

using should_quit_function_type = function<bool ()>;
kowned_ptr<ipv4_device> make_ipv4_device(ethernet_device& ethdev);
bool do_dhcp(ipv4_device& ipv4dev, should_quit_function_type should_quit);

} }

namespace attos { namespace net { namespace tftp {

constexpr uint16_t dst_port = 69;

constexpr uint32_t block_size = 512;

constexpr bool legal_size(uint64_t size) {
    return size <= block_size*(UINT16_MAX-1);
}

constexpr uint16_t block_count(uint64_t size) {
    return static_cast<uint16_t>((size + 512) / tftp::block_size);
}


enum class opcode : uint16_t {
    rrq   = 1, // Read request (RRQ)
    wrq   = 2, // Write request (WRQ)
    data  = 3, // Data (DATA)
    ack   = 4, // Acknowledgment (ACK)
    error = 5, // Error (ERROR)
};

enum class error_code : uint16_t {
    not_defined       = 0, // Not defined, see error message (if any).
    file_not_found    = 1, // File not found.
    access_violation  = 2, // Access violation.
    disk_full         = 3, // Disk full or allocation exceeded.
    illegal_operation = 4, // Illegal TFTP operation.
    unknown_id        = 5, // Unknown transfer ID.
    file_exists       = 6, // File already exists.
    no_such_user      = 7, // No such user.
};

inline uint8_t* put(uint8_t* b, uint16_t x) {
    b[0] = static_cast<uint8_t>(x>>8);
    b[1] = static_cast<uint8_t>(x);
    return b + 2;
}

inline uint8_t* put(uint8_t* b, opcode op) {
    return put(b, static_cast<uint16_t>(op));
}

uint8_t* put(uint8_t* b, const char* s);

uint8_t* put_error_reply(uint8_t* b, error_code ec, const char* msg);

uint16_t get_u16(const uint8_t*& data, uint32_t& length);
opcode get_opcode(const uint8_t*& data, uint32_t& length);
const char* get_string(const uint8_t*& data, uint32_t& length);

kvector<uint8_t> read(ipv4_device& ipv4dev, should_quit_function_type should_quit, const char* filename);
bool write(ipv4_device& ipv4dev, should_quit_function_type should_quit, const char* filename, array_view<uint8_t> data);


} } } // namespace attos::net::tftp

#endif

#include <attos/out_stream.h>
#include <attos/string.h>
#include <attos/net/net.h>
#include <attos/net/tftp.h>
#include <attos/cpu.h>

using namespace attos;
using namespace attos::net;

class tftp_server_session {
public:
    explicit tftp_server_session(ipv4_address remote_addr, uint16_t remote_port) : remote_addr_(remote_addr), remote_port_(remote_port) {
    }

    tftp_server_session(const tftp_server_session&) = delete;
    tftp_server_session& operator=(const tftp_server_session&) = delete;

    ipv4_address remote_addr() const { return remote_addr_; }
    uint16_t     remote_port() const { return remote_port_; }

private:
    const ipv4_address remote_addr_;
    const uint16_t     remote_port_;
};

#include <winsock2.h>
#include <memory>
#include <vector>

int main()
{
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
        dbgout() << "Error initializing winsock\n";
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        dbgout() << "socket() failed: " << WSAGetLastError() << "\n";
        return 2;
    }

    sockaddr_in local={};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = 0;
    local.sin_port = htons(69);

    if (bind(sock, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
        dbgout() << "bind() failed: " << WSAGetLastError() << "\n";
        return 3;
    }

    dbgout() << "Server running on port " << htons(local.sin_port) << "\n";
    std::vector<std::unique_ptr<tftp_server_session>> sessions_;
    for (bool done = false; !done;) {
        sockaddr_in addr;
        int addr_len = sizeof(addr);

        char buf[1024];
        int n = recvfrom(sock, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&addr), &addr_len);
        if (n < 0) {
            dbgout() << "recvfrom() failed: " << WSAGetLastError() << "\n";
            break;
        }
        const auto remote_addr = ipv4_address::host_u32(htonl(addr.sin_addr.s_addr));
        REQUIRE(remote_addr != inaddr_broadcast);
        const auto remote_port = htons(addr.sin_port);

        auto it = std::find_if(sessions_.begin(), sessions_.end(), [=](const std::unique_ptr<tftp_server_session>& s) { return s->remote_addr() == remote_addr && s->remote_port() == remote_port; });

        auto in = reinterpret_cast<const uint8_t*>(buf);
        uint32_t in_len = static_cast<uint32_t>(n);


        uint8_t out_buffer[512+4];
        uint8_t* b = out_buffer;

        const auto opcode = tftp::get_opcode(in, in_len);
        switch (opcode) {
        case tftp::opcode::rrq:
            {
                auto filename = tftp::get_string(in, in_len);
                auto mode = tftp::get_string(in, in_len);
                dbgout() << "[tftp] RRQ from " << remote_addr << ":" << remote_port << " file '" << filename << " mode '" << mode << "'\n";
                if (!string_equal(mode, "octet")) {
                    b = tftp::put_error_reply(b, tftp::error_code::illegal_operation, "Unsupported RRQ mode");
                    break;
                }
                if (it != sessions_.end()) {
                    dbgout() << "[tftp] Closing previous sessions for " << (*it)->remote_addr() << ":" << (*it)->remote_port() << "\n";
                    sessions_.erase(it);
                }
                sessions_.push_back(std::make_unique<tftp_server_session>(remote_addr, remote_port));

                b = tftp::put(b, tftp::opcode::data);
                b = tftp::put(b, uint16_t{1});
                b = tftp::put(b, "Hello world!\n");

                break;
            }
        case tftp::opcode::ack:
            {
                const auto block_number = tftp::get_u16(in, in_len);
                dbgout() << "[tftp] ACK from " << remote_addr << ":" << remote_port << " Block #" << block_number << "\n";
            }
            break;
        default:
            dbgout() << "Got unsupported request " << static_cast<uint16_t>(opcode) << "\n";
            hexdump(dbgout(), buf, n);
            done = true;
            b = tftp::put_error_reply(b, tftp::error_code::illegal_operation, "Unsupported TFTP opcode");
        }

        if (b != out_buffer) {
            if (sendto(sock, reinterpret_cast<const char*>(out_buffer), static_cast<int>(b - out_buffer), 0, reinterpret_cast<const sockaddr*>(&addr), addr_len) != (b - out_buffer)) {
                dbgout() << "sendto() failed: " << WSAGetLastError() << "\n";
                break;
            }
        }
    }

    closesocket(sock);
    WSACleanup();
};

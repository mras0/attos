#include <attos/out_stream.h>
#include <attos/string.h>
#include <attos/net/net.h>
#include <attos/net/tftp.h>
#include <attos/cpu.h>

using namespace attos;
using namespace attos::net;

class tftp_server_session {
public:
    explicit tftp_server_session(ipv4_address remote_addr, uint16_t remote_port, kvector<uint8_t>&& data) : remote_addr_(remote_addr), remote_port_(remote_port), data_(std::move(data)) {
    }

    tftp_server_session(const tftp_server_session&) = delete;
    tftp_server_session& operator=(const tftp_server_session&) = delete;

    ipv4_address remote_addr() const { return remote_addr_; }
    uint16_t     remote_port() const { return remote_port_; }

    uint16_t block_count() {
        return static_cast<uint16_t>((data_.size() + 512) / tftp::block_size);
    }

    uint8_t* put_block(uint8_t* b, uint16_t block) {
        REQUIRE(block >= 1 && block <= block_count());
        b = tftp::put(b, tftp::opcode::data);
        b = tftp::put(b, block);

        const auto offset = (block - 1) * tftp::block_size;
        const auto count  = std::min(tftp::block_size, static_cast<uint32_t>(data_.size() - offset));
        memcpy(b, data_.begin() + offset, count);
        b += count;
        dbgout() << "[tftp] DATA " << remote_addr_ << ":" << remote_port_ << " Block #" << block << " Size " << count << "\n";
        return b;
    }

private:
    const ipv4_address       remote_addr_;
    const uint16_t           remote_port_;
    const kvector<uint8_t>   data_;
};

#include <winsock2.h>
#include <memory>
#include <vector>
#include <string>
#include <fstream>

std::string exe_dir()
{
    char buffer[MAX_PATH];
    REQUIRE(GetModuleFileNameA(nullptr, buffer, _countof(buffer)));
    for (int i = static_cast<int>(string_length(buffer)); i >= 0; --i) {
        if (buffer[i] == '\\' || buffer[i] == '/') {
            buffer[i] = '\0';
            return buffer;
        }
    }
    REQUIRE(false);
    return "";
}

bool valid_filename(const char* filename)
{
    for (int pos = 0; filename[pos]; ++pos) {
        const char c = filename[pos];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (pos && c == '.')) {
            continue;
        }
        dbgout() << "Illegal char '" << c << "' (0x" << as_hex((unsigned char)c) << ")\n";
        return false;
    }
    return true;
}

bool read_file(const char* filename, kvector<uint8_t>& data)
{
    std::ifstream in((exe_dir() + "\\" + filename).c_str(), std::ifstream::binary);
    if (!in.is_open()) {
        return false;
    }
    in.seekg(0, std::ios::end);
    std::vector<char> buf(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&buf[0], buf.size());
    data = kvector<uint8_t>(buf.data(), buf.data() + buf.size());
    return true;
}

int main()
{
    dbgout() << "exe_dir = '" << exe_dir().c_str() << "'\n";

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
                if (it != sessions_.end()) {
                    dbgout() << "[tftp] Closing previous sessions for " << (*it)->remote_addr() << ":" << (*it)->remote_port() << "\n";
                    sessions_.erase(it);
                }
                if (!string_equal(mode, "octet")) {
                    dbgout() << "[tftp] Unsupported mode\n";
                    b = tftp::put_error_reply(b, tftp::error_code::illegal_operation, "Unsupported RRQ mode");
                    break;
                }
                if (!valid_filename(filename)) {
                    dbgout() << "[tftp] Invalid filename\n";
                    b = tftp::put_error_reply(b, tftp::error_code::file_not_found, "Invalid filename");
                    break;
                }
                kvector<uint8_t> data;
                if (!read_file(filename, data)) {
                    dbgout() << "[tftp] File not found\n";
                    b = tftp::put_error_reply(b, tftp::error_code::file_not_found, "File not found");
                    break;
                }
                sessions_.push_back(std::make_unique<tftp_server_session>(remote_addr, remote_port, std::move(data)));
                b = sessions_.back()->put_block(b, 1);
                break;
            }
        case tftp::opcode::ack:
            {
                const auto block_number = tftp::get_u16(in, in_len);
                dbgout() << "[tftp] ACK from " << remote_addr << ":" << remote_port << " Block #" << block_number << "\n";
                REQUIRE(it != sessions_.end());
                auto& session = **it;
                REQUIRE(block_number >= 1 && block_number <= session.block_count());
                if (block_number == session.block_count()) {
                    sessions_.erase(it);
                    dbgout() << "[tftp] Transfer done.\n";
                } else {
                    b = session.put_block(b, block_number + 1);
                }
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

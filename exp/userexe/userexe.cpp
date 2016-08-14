#include <attos/out_stream.h>
#include <attos/net/tftp.h>
#include <attos/cpu.h>
#include <attos/string.h>
#include <attos/syscall.h>

using namespace attos;
using namespace attos::net;

class sys_handle {
public:
    explicit sys_handle(const char* name) : id_(syscall1(syscall_number::create, (uint64_t)name)) {
    }
    ~sys_handle() {
        syscall1(syscall_number::destroy, id_);
    }
    sys_handle(const sys_handle&) = delete;
    sys_handle& operator=(const sys_handle&) = delete;

    uint64_t id() const { return id_; }

private:
    uint64_t id_;
};

void write(sys_handle& h, const void* data, uint32_t length) {
    syscall3(syscall_number::write, h.id(), (uint64_t)data, length);
}

uint32_t read(sys_handle& h, void* data, uint32_t max) {
    return static_cast<uint32_t>(syscall3(syscall_number::read, h.id(), (uint64_t)data, max));
}

class my_ethernet_device : public ethernet_device {
public:
    explicit my_ethernet_device() : handle_("ethdev") {
    }
    virtual ~my_ethernet_device() override {
    }
private:
    sys_handle handle_;

    virtual mac_address do_hw_address() const override {
        mac_address ma;
        syscall2(syscall_number::ethdev_hw_address, handle_.id(), (uint64_t)&ma);
        return ma;
    }
    virtual void do_send_packet(const void* data, uint32_t length) override {
        write(handle_, data, length);
    }
    virtual void do_process_packets(const packet_process_function& ppf, int max_packets) override {
        for (int i = 0; i < max_packets; ++i) {
            uint8_t  buffer[2048];
            if (const uint32_t len = read(handle_, buffer, sizeof(buffer))) {
                ppf(buffer, static_cast<uint32_t>(len));
            }
        }
    }
};

class my_keyboard : public singleton<my_keyboard> {
public:
    explicit my_keyboard() : handle_("keyboard") {
    }

    ~my_keyboard() {
    }

    bool key_available() {
        poll();
        return key != no_key;
    }

    uint8_t read_key() {
        while (key == no_key) {
            yield();
            poll();
        }
        const auto c = static_cast<uint8_t>(key);
        key = no_key;
        return c;
    }
private:
    sys_handle handle_;
    static constexpr int no_key = -1;
    int key = no_key;

    void poll() {
        if (key == no_key) {
            uint8_t c;
            if (read(handle_, &c, 1)) {
                key = c;
            }
        }
    }
};

template<typename T>
struct push_back_stream_adapter : public out_stream {
    explicit push_back_stream_adapter(T& c) : c_(c) {
    }

    virtual void write(const void* data, size_t size) override {
        auto src = reinterpret_cast<const uint8_t*>(data);
        while (size--) {
            c_.push_back(*src++);
        }
    }

private:
    T& c_;
};

constexpr int cmd_max = 40;

void clearline()
{
    const char buf[] = "\r                                        \r";
    static_assert(sizeof(buf)==cmd_max+3,"Lazy..");
    dbgout() << buf;
}

bool escape_pressed()
{
    auto& kbd = my_keyboard::instance();
    return kbd.key_available() && kbd.read_key() == '\x1b';
}

void tftp_execute(ipv4_device& ipv4dev, const char* filename)
{
    auto data = tftp::nettest(ipv4dev, &escape_pressed, filename);
    if (!data.empty()) {
        hexdump(dbgout(), data.begin(), data.size());
        sys_handle proc{"process"};
        syscall2(syscall_number::start_exe, proc.id(), (uint64_t)data.begin());
        dbgout() << filename << " exited with error code " << as_hex(syscall1(syscall_number::process_exit_code, proc.id())) << "!\n";
    } else {
        dbgout() << "Failed/aborted\n";
    }
}

int main()
{
    my_keyboard kbd;
    my_ethernet_device ethdev;
    auto ipv4dev = net::make_ipv4_device(ethdev);
    do_dhcp(*ipv4dev, &escape_pressed);
    tftp_execute(*ipv4dev, "test.exe");
    dbgout() << "Interactive mode. Use escape to quit.\n";

    kvector<char> cmd;
    push_back_stream_adapter<kvector<char>> cmd_stream(cmd);
    for (;;) {
        auto k = kbd.read_key();
        if (k == '\x1b') { // ESC
            dbgout() << "\nEscape pressed\n";
            break;
        } else if (k == '\x08') { // BS
            if (!cmd.empty()) {
                cmd.back() = '\0';
                cmd.pop_back();
                clearline();
                dbgout() << cmd.begin();
            }
        } else if (k == '\n') {
            if (!cmd.empty()) {
                cmd.push_back('\0');
                dbgout() << '\n';
                if (string_equal(cmd.begin(), "EXIT")) {
                    dbgout() << "Exit\n";
                    break;
                } else if (cmd.size() > 3 && cmd[0] == 'X' && cmd[1] == ' ') {
                    dbgout() << "Execute '" << &cmd[2] << "'\n";
                    tftp_execute(*ipv4dev, &cmd[2]);
                } else {
                    dbgout() << "COMMAND IGNORED: '" << cmd.begin() << "'\n";
                }
                cmd.clear();
            }
        } else {
            if (cmd.size() < cmd_max) {
                cmd_stream << (char)k;
                dbgout() << (char)k;
            }
        }
    }
}

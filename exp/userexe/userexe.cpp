#include <attos/out_stream.h>
#include <attos/net/tftp.h>
#include <attos/cpu.h>
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

class my_keyboard {
public:
    explicit my_keyboard() : handle_("keyboard") {
    }

    ~my_keyboard() {
    }

    bool esc_pressed() {
        char c;
        return read(handle_, &c, 1) && c == '\x1b';
    }

private:
    sys_handle handle_;
};

int main()
{
    my_keyboard keyboard;
    my_ethernet_device ethdev;
    dbgout() << "HW address: " << ethdev.hw_address() << "\n";
    tftp::nettest(ethdev, [&] { return keyboard.esc_pressed(); }, "test.txt");
}

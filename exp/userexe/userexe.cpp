#include <attos/out_stream.h>
#include <attos/net/tftp.h>
#include <attos/cpu.h>
#include <attos/syscall.h>

using namespace attos;
using namespace attos::net;

class my_ethernet_device : public ethernet_device {
public:
    explicit my_ethernet_device() {
        syscall1(syscall_number::ethdev_create, (uint64_t)&id_);
        dbgout() << "[my_ethernet_device] constructed id " << as_hex(id_) << "\n";
    }
    virtual ~my_ethernet_device() override {
        dbgout() << "[my_ethernet_device] destructing id " << as_hex(id_) << "\n";
        syscall1(syscall_number::ethdev_destroy, id_);
    }
private:
    uint64_t id_;

    virtual mac_address do_hw_address() const override {
        mac_address ma;
        syscall2(syscall_number::ethdev_hw_address, id_, (uint64_t)&ma);
        return ma;
    }
    virtual void do_send_packet(const void* data, uint32_t length) override {
        syscall3(syscall_number::ethdev_send, id_, (uint64_t)data, (uint64_t)length);
    }
    virtual void do_process_packets(const packet_process_function& ppf, int max_packets) override {
        for (int i = 0; i < max_packets; ++i) {
            uint8_t  buffer[2048];
            uint32_t len = 0;
            syscall3(syscall_number::ethdev_recv, id_, (uint64_t)buffer, (uint64_t)&len);
            if (!len) break;
            ppf(buffer, len);
        }
    }
};

int main()
{
    my_ethernet_device ethdev;
    dbgout() << "HW address: " << ethdev.hw_address() << "\n";
    tftp::nettest(ethdev, [] { return syscall0(syscall_number::esc_pressed) != 0; }, "test.txt");
}

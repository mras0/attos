#include <attos/out_stream.h>
#include <attos/net/net.h>
#include <attos/cpu.h>

using namespace attos;
using namespace attos::net;

class my_ethernet_device : public ethernet_device {
public:
    explicit my_ethernet_device() {
        dbgout() << "[my_ethernet_device] constructing...\n";
    }
    virtual ~my_ethernet_device() override {
        dbgout() << "[my_ethernet_device] destructing...\n";
    }


private:
    virtual mac_address do_hw_address() const override {
        dbgout() << "[my_ethernet_device] do_hw_address\n";
        REQUIRE(false);
    }
    virtual void do_send_packet(const void* data, uint32_t length) override {
        dbgout() << "[my_ethernet_device] data " << as_hex((uint64_t)data) << " length " << as_hex(length) << "\n";
        REQUIRE(false);
    }
    virtual void do_process_packets(const packet_process_function& ppf) override {
        (void)ppf;
        dbgout() << "[my_ethernet_device] do_process_packets\n";
        REQUIRE(false);
    }
};

int main()
{
    dbgout() << "Hello world from user mode answer=" << 42 << "\n";
    // my_ethernet_device ethdev;
    //nettest(ethdev, [] { return false; });

    return 0xfede0abe;
}

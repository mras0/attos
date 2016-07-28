#include "i8254.h"
#include <attos/cpu.h>
#include <attos/out_stream.h>

namespace attos { namespace net {

class i8254 : public netdev {
public:
    explicit i8254(uint64_t iobase) {
        dbgout() << "[i8254] Initializing. IOBASE = " << as_hex(iobase).width(8) << "\n";
    }
    virtual ~i8254() override {
        dbgout() << "[i8254] Shutting down\n";
    }
};


netdev_ptr i82545_probe(const pci::device_info& dev_info)
{
    constexpr uint16_t i82540em_a = 0x100e; // desktop
    constexpr uint16_t i82545em_a = 0x100f; // copper

    if (dev_info.config.vendor_id == pci::vendor::intel && (dev_info.config.device_id == i82540em_a || dev_info.config.device_id == i82545em_a)) {
        REQUIRE(!(dev_info.bars[0].address & pci::bar_is_io_mask)); // Register base address
        REQUIRE(dev_info.bars[0].size == (128<<10));
        return netdev_ptr{knew<i8254>(dev_info.bars[0].address&pci::bar_mem_address_mask).release()};
    }

    return netdev_ptr{};
}

} } // namespace attos::net

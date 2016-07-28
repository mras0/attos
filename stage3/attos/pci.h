#ifndef ATTOS_PCI_H
#define ATTOS_PCI_H

#include <attos/mem.h>
#include <attos/array_view.h>
#include <array>

namespace attos {
class out_stream;
} // namespace attos

namespace attos { namespace pci {

class device_address {
public:
    explicit device_address(uint8_t bus, uint8_t slot, uint8_t func);

    constexpr uint8_t bus() const  { return (address_>>16) & 0xFF; }
    constexpr uint8_t slot() const { return (address_>>11) & 0x1F; }
    constexpr uint8_t func() const { return (address_>>8) & 0x7;   }

    constexpr explicit operator uint32_t() const { return address_; }

private:
    uint32_t address_;
};

out_stream& operator<<(out_stream& os, device_address dev);

constexpr uint32_t bar_is_io_mask        = 0x01;
constexpr uint32_t bar_mem_type_mask     = 0x06;
constexpr uint32_t bar_prefatchable_mask = 0x08;
constexpr uint64_t bar_mem_address_mask  = ~15ULL;
constexpr uint64_t bar_io_address_mask   = ~3ULL;

struct bar_info {
    uint64_t address;
    uint64_t size;
};

using all_bars = std::array<bar_info, 6>;

enum class vendor : uint16_t {
   lsi     = 0x1000, // LSI Logic / Symbios Logic
   ensoniq = 0x1274, // Ensoniq
   vmware  = 0x15AD, // VMware Inc
   intel   = 0x8086, // Intel Corporation
};

enum class device_class : uint16_t {
    scsi_controller             = 0x0100,
    ide_controller              = 0x0101,
    serial_ata_controller       = 0x0106,

    ethernet_controller         = 0x0200,
    other_network_controller    = 0x0280,

    vga_controller              = 0x0300,
    other_display_controller    = 0x0380,

    bidi_audio_device           = 0x0401, // Handset - Hand-held bi-directional audio device
    audio_device                = 0x0403, // Speakphone - A hands-free audio device designed for host-based echo cancellation.

    host_bridge                 = 0x0600,
    isa_bridge                  = 0x0601,
    pci_bridge                  = 0x0604,
    other_bridge                = 0x0680,

    generic_serial_controller   = 0x0700,
    other_communications_device = 0x0780,

    other_system_peripheral     = 0x0880,

    usb_controller              = 0x0c03,
    smbus                       = 0x0c05,
};

#pragma pack(push, 1)
struct config_area {
    vendor   vendor_id;
    uint16_t device_id;

    uint16_t command;
    uint16_t status;

    uint8_t  rev_id;
    uint8_t  prog_if;
    //uint8_t  subclass;
    //uint8_t  class_code;
    device_class dev_class;

    uint8_t  cache_line_size;
    uint8_t  latency_timer;
    uint8_t  header_type;
    uint8_t  bist;

    union {
        struct {
            uint32_t bar[6];
            uint32_t cardbus_ptr;

            vendor   subsys_vendor;
            uint16_t subsys_id;

            uint32_t expansion_rom;

            uint8_t  caps;
            uint8_t  reserved[7];

            uint8_t  intr_line;
            uint8_t  intr_pin;
            uint8_t  min_grant;
            uint8_t  max_latency;
        } header0;
        struct {
            uint32_t bar[2];
            uint8_t  primary_bus;
            uint8_t  secondary_bus;
            uint8_t  subordinate_bus;
            uint8_t  secondary_latency_timer;
        } header1; // PCI-to-PCI bridge
        uint32_t remaining[60]; // ensure sizeof(config_area) == 256
    };
};
#pragma pack(pop)

struct device_info {
    device_address  address;
    config_area     config;
    all_bars        bars;
};

class __declspec(novtable) manager {
public:
    virtual ~manager() {}

    array_view<device_info> devices() const {
        return do_devices();
    }

private:
    virtual array_view<device_info> do_devices() const = 0;
};

owned_ptr<manager, destruct_deleter> init();

} }  // namespace attos::pci

#endif

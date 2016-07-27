#include "pci.h"
#include <attos/out_stream.h>
#include <attos/cpu.h>
#include <attos/mm.h>
#include <array>

namespace attos { extern uint8_t read_key(); }

namespace attos { namespace pci {

constexpr int config_area_num_dwords = 64;

constexpr uint16_t config_address_port = 0xCF8;
constexpr uint16_t config_data_port    = 0xCFC;

class device_address {
public:
    explicit device_address(uint8_t bus, uint8_t slot, uint8_t func) : address_((bus << 16) | (slot << 11) | (func << 8)) {
        REQUIRE(slot < 32);
        REQUIRE(func < 8);
    }

    uint8_t bus() const  { return (address_>>16) & 0xFF; }
    uint8_t slot() const { return (address_>>11) & 0x1F; }
    uint8_t func() const { return (address_>>8) & 0x7;   }

    explicit operator uint32_t() const { return address_; }

private:
    uint32_t address_;
};

out_stream& operator<<(out_stream& os, device_address dev) {
    return os << as_hex(dev.bus()) << ":" << as_hex(dev.slot()) << "." << as_hex(dev.func()).width(1);
}

uint32_t read_config_dword(device_address addr, uint8_t reg)
{
    REQUIRE(reg < config_area_num_dwords);
// 31	        30 - 24	    23 - 16	    15 - 11	        10 - 8	        7 - 2	        1 - 0
// Enable Bit	Reserved	Bus Number	Device Number	Function Number	Register Number	00
    __outdword(config_address_port, 0x8000'0000 | static_cast<uint32_t>(addr) | (reg << 2));
    return __indword(config_data_port);
}

void write_config_dword(device_address addr, uint8_t reg, uint32_t value)
{
    REQUIRE(reg < config_area_num_dwords);
    __outdword(config_address_port, 0x8000'0000 | static_cast<uint32_t>(addr) | (reg << 2));
    __outdword(config_data_port, value);
}
constexpr uint8_t header_type_device_mask         = 0x03; // 0x00 = general device, 0x01 = PCI-to-PCI bridge, 0x02 = CardBus bridge
constexpr uint8_t header_type_multi_function_mask = 0x80;

#pragma pack(push, 1)
struct config_area {
    uint16_t vendor;
    uint16_t device_id;

    uint16_t command;
    uint16_t status;

    uint8_t  rev_id;
    uint8_t  prog_if;
    //uint8_t  subclass;
    //uint8_t  class_code;
    uint16_t device_class;

    uint8_t  cache_line_size;
    uint8_t  latency_timer;
    uint8_t  header_type;
    uint8_t  bist;

    union {
        struct {
            uint32_t bar[6];
            uint32_t cardbus_ptr;

            uint16_t subsys_vendor;
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
static_assert(sizeof(config_area) == config_area_num_dwords * 4, "");

constexpr uint16_t vendor_lsi     = 0x1000; // LSI Logic / Symbios Logic
constexpr uint16_t vendor_ensoniq = 0x1274; // Ensoniq
constexpr uint16_t vendor_vmware  = 0x15AD; // VMware Inc
constexpr uint16_t vendor_intel   = 0x8086; // Intel Corporation

constexpr uint16_t device_class_scsi_controller             = 0x0100;
constexpr uint16_t device_class_ide_controller              = 0x0101;
constexpr uint16_t device_class_serial_ata_controller       = 0x0106;

constexpr uint16_t device_class_ethernet_controller         = 0x0200;
constexpr uint16_t device_class_other_network_controller    = 0x0280;

constexpr uint16_t device_class_vga_controller              = 0x0300;
constexpr uint16_t device_class_other_display_controller    = 0x0380;

constexpr uint16_t device_class_bidi_audio_device           = 0x0401; // Handset - Hand-held bi-directional audio device
constexpr uint16_t device_class_audio_device                = 0x0403; // Speakphone - A hands-free audio device designed for host-based echo cancellation.

constexpr uint16_t device_class_host_bridge                 = 0x0600;
constexpr uint16_t device_class_isa_bridge                  = 0x0601;
constexpr uint16_t device_class_pci_bridge                  = 0x0604;
constexpr uint16_t device_class_other_bridge                = 0x0680;

constexpr uint16_t device_class_generic_serial_controller   = 0x0700;
constexpr uint16_t device_class_other_communications_device = 0x0780;

constexpr uint16_t device_class_other_system_peripheral     = 0x0880;

constexpr uint16_t device_class_usb_controller              = 0x0c03;
constexpr uint16_t device_class_smbus                       = 0x0c05;

struct vendor_info {
    uint16_t vendor;
    const char* text;
};
const vendor_info known_vendors[] = {
    { vendor_lsi     , "LSI Logic / Symbios Logic" },
    { vendor_ensoniq , "Ensoniq"                   },
    { vendor_vmware  , "VMware Inc"                },
    { vendor_intel   , "Intel Corporation"         },
};

struct device_info {
    uint16_t vendor;
    uint16_t device_id;
    uint16_t device_class;
    const char* text;
};
const device_info known_devices[] = {
    { vendor_lsi     , 0x0030 , device_class_scsi_controller              , "53c1030 PCI-X Fusion-MPT Dual Ultra320 SCSI"   },
    { vendor_ensoniq , 0x1371 , device_class_bidi_audio_device            , "ES1371 / Creative Labs CT2518/ES1373"          },
    { vendor_vmware  , 0x0405 , device_class_vga_controller               , "SVGA II Adapter"                               },
    { vendor_vmware  , 0x0740 , device_class_other_system_peripheral      , "Virtual Machine Communication Interface"       },
    { vendor_vmware  , 0x0790 , device_class_pci_bridge                   , "PCI bridge"                                    },
    { vendor_vmware  , 0x07A0 , device_class_pci_bridge                   , "PCI Express Root Port"                         },
    { vendor_intel   , 0x100E , device_class_ethernet_controller          , "82540EM Gigabit Ethernet Controller"           },
    { vendor_intel   , 0x100F , device_class_ethernet_controller          , "82545EM Gigabit Ethernet Controller (Copper)"  },
    { vendor_intel   , 0x10f5 , device_class_ethernet_controller          , "82567LM Gigabit Network Connection"            },
    { vendor_intel   , 0x1237 , device_class_host_bridge                  , "440FX - 82441FX PMC [Natoma]"                  },
    { vendor_intel   , 0x2448 , device_class_pci_bridge                   , "82801 Mobile PCI Bridge"                       },
    { vendor_intel   , 0x2917 , device_class_isa_bridge                   , "ICH9M-E LPC Interface Controller"              },
    { vendor_intel   , 0x2929 , device_class_serial_ata_controller        , "82801IBM/IEM SATA Controller"                  }, // 82801IBM/IEM (ICH9M/ICH9M-E) 4 port SATA Controller [AHCI mode]
    { vendor_intel   , 0x2930 , device_class_smbus                        , "82801I (ICH9 Family) SMBus Controller"         }, 
    { vendor_intel   , 0x2934 , device_class_usb_controller               , "82801I (ICH9 Family) USB UHCI Controller #1"   },
    { vendor_intel   , 0x2935 , device_class_usb_controller               , "82801I (ICH9 Family) USB UHCI Controller #2"   },
    { vendor_intel   , 0x2936 , device_class_usb_controller               , "82801I (ICH9 Family) USB UHCI Controller #3"   },
    { vendor_intel   , 0x2937 , device_class_usb_controller               , "82801I (ICH9 Family) USB UHCI Controller #4"   },
    { vendor_intel   , 0x2938 , device_class_usb_controller               , "82801I (ICH9 Family) USB UHCI Controller #5"   },
    { vendor_intel   , 0x2939 , device_class_usb_controller               , "82801I (ICH9 Family) USB UHCI Controller #6"   },
    { vendor_intel   , 0x293a , device_class_usb_controller               , "82801I (ICH9 Family) USB2 EHCI Controller #1"  },
    { vendor_intel   , 0x293c , device_class_usb_controller               , "82801I (ICH9 Family) USB2 EHCI Controller #2"  },
    { vendor_intel   , 0x293e , device_class_audio_device                 , "82801I (ICH9 Family) HD Audio Controller"      },
    { vendor_intel   , 0x2a40 , device_class_host_bridge                  , "Mobile 4 Series Chipset Memory Controller Hub" },
    { vendor_intel   , 0x2a42 , device_class_vga_controller               , "Mobile 4 Series Chipset Integrated Graphics"   }, // Mobile 4 Series Chipset Integrated Graphics Controller
    { vendor_intel   , 0x2a43 , device_class_other_display_controller     , "Mobile 4 Series Chipset Integrated Graphics"   }, // Mobile 4 Series Chipset Integrated Graphics Controller
    { vendor_intel   , 0x2a44 , device_class_other_communications_device  , "Mobile 4 Series Chipset MEI Controller"        },
    { vendor_intel   , 0x2a46 , device_class_ide_controller               , "Mobile 4 Series Chipset PT IDER Controller"    },
    { vendor_intel   , 0x2a47 , device_class_generic_serial_controller    , "Mobile 4 Series Chipset AMT SOL Redirection"   },
    { vendor_intel   , 0x4237 , device_class_other_network_controller     , "PRO/Wireless 5100 AGN Network Connection"      }, // PRO/Wireless 5100 AGN [Shiloh] Network Connection
    { vendor_intel   , 0x7000 , device_class_isa_bridge                   , "82371SB PIIX3 ISA [Natoma/Triton II]"          },
    { vendor_intel   , 0x7010 , device_class_ide_controller               , "82371SB PIIX3 IDE [Natoma/Triton II]"          },
    { vendor_intel   , 0x7020 , device_class_usb_controller               , "82371SB PIIX3 USB [Natoma/Triton II]"          },
    { vendor_intel   , 0x7110 , device_class_isa_bridge                   , "82371AB/EB/MB PIIX4 ISA"                       },
    { vendor_intel   , 0x7111 , device_class_ide_controller               , "82371AB/EB/MB PIIX4 IDE"                       },
    { vendor_intel   , 0x7113 , device_class_other_bridge                 , "82371AB/EB/MB PIIX4 ACPI"                      },
    { vendor_intel   , 0x7190 , device_class_host_bridge                  , "440BX/ZX/DX - 82443BX/ZX/DX Host bridge"       },
    { vendor_intel   , 0x7191 , device_class_pci_bridge                   , "440BX/ZX/DX - 82443BX/ZX/DX AGP bridge"        },
};

template<typename T, size_t size, typename Pred>
const T* find_info(const T (&arr)[size], Pred p) {
    auto it = std::find_if(std::begin(arr), std::end(arr), p);
    return it == std::end(arr) ? nullptr : &*it;
}

const vendor_info* get_vendor_info(uint16_t vendor) {
    return find_info(known_vendors, [vendor](const vendor_info& vi) { return vi.vendor == vendor; });
}

const device_info* get_device_info(const config_area& ca) {
    return find_info(known_devices, [&ca](const device_info& di) { return ca.vendor == di.vendor && ca.device_id == di.device_id && ca.device_class == di.device_class; });
}

config_area read_config_area(device_address dev)
{
    union {
        config_area ca;
        uint32_t    dws[config_area_num_dwords];
    } u;
    for (int i = 0; i < config_area_num_dwords; ++i) {
        u.dws[i] = read_config_dword(dev, static_cast<uint8_t>(i));
    }
    return u.ca;
}

struct bar_info {
    uint64_t address;
    uint32_t size;
};

std::array<bar_info, 6> read_bars(device_address addr) {
    std::array<bar_info, 6> res;

    for (uint8_t i = 0; i < 6; ++i) {
        const uint8_t reg = 4 + i; // bar0 is at offset 4

        // Save original value
        const auto orig = read_config_dword(addr, reg);

        res[i].address = orig;
        res[i].size    = 0;

        if (!orig) {
            continue;
        }

        // Determine size by writing all 1s and seeing what sticks (taking care that only the lower 16 bits are valid for IO bars)
        write_config_dword(addr, reg, ~0U);
        auto bar_size = read_config_dword(addr, reg);

        // Restore original bar value back
        write_config_dword(addr, reg, orig);

        // Mask of R/O bits
        if (orig & 1) {
            bar_size &= ~3;
        } else {
            bar_size &= ~15;
        }

        // Invert bits and increment 1
        res[i].size = (~bar_size) + 1;
    }
    return res;
}

class manager_impl : public manager {
public:
    manager_impl() {
        dbgout() << "[pci] Initializing.\n";
        probe_bus(0);
    }
    ~manager_impl() {
        dbgout() << "[pci] Shutting down.\n";
    }
private:
    void probe(device_address dev_addr) {
        const auto cfgdw = read_config_dword(dev_addr, 0);
        if ((cfgdw&0xffff) == 0xffff) {
            return;
        }
        const auto ca = read_config_area(dev_addr);
        const auto header_type = ca.header_type & header_type_device_mask;
        if (header_type == 0) {
            // 0x00 = Normal devices
            dbgout() << dev_addr << " " << as_hex(ca.device_class) << ": ";
            if (auto vi = get_vendor_info(ca.vendor)) {
                dbgout() << vi->text;
            } else {
                dbgout() << "Vendor " << as_hex(ca.vendor);
            }
            dbgout() << " ";
            if (auto di = get_device_info(ca)) {
                dbgout() << di->text;
            } else {
                dbgout() << "Device " << as_hex(ca.device_id);
            }
            dbgout() << "\n";
#if 0
            if (ca.device_class == device_class_ide_controller) {
                for (const auto& b: read_bars(dev_addr)) {
                    if (!b.size) continue;
                    if (b.address & 1) dbgout() << " IO  " << as_hex(b.address&~3).width(4);
                    else dbgout() << " MEM " << as_hex(b.address&~15).width(8);
                    dbgout() << " size " << as_hex(b.size).width(0) << "\n";
                }
                dbgout() << " ProgIF: " << as_hex(ca.prog_if) << "\n";
            }
#endif
        } else if (header_type == 1) {
            // 0x01 = PCI-to-PCI bridge
            REQUIRE(dev_addr.bus() == ca.header1.primary_bus);
            probe_bus(ca.header1.secondary_bus);
        } else {
            // 0x02 = CardBus bridge
            dbgout() << dev_addr << " Unknown header type " << as_hex(ca.header_type) << "\n";
            REQUIRE(false);
        }

        if (dev_addr.func() == 0 && (ca.header_type & header_type_multi_function_mask)) {
            for (uint8_t func = 1; func < 8; ++func) {
                probe(device_address{dev_addr.bus(), dev_addr.slot(), func});
            }
        }
    }

    void probe_bus(uint8_t bus) {
        for (int slot = 0; slot < 32; ++slot) {
            const auto dev_addr = device_address{static_cast<uint8_t>(bus), static_cast<uint8_t>(slot), 0};
            probe(dev_addr);
        }
    }

};

object_buffer<manager_impl> manager_buffer;

owned_ptr<manager, destruct_deleter> init() {
    return owned_ptr<manager, destruct_deleter>{manager_buffer.construct().release()};
}

} } // namespace attos::pci

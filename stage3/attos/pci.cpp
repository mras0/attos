#include "pci.h"
#include <attos/out_stream.h>
#include <attos/cpu.h>
#include <attos/mm.h>

namespace attos { namespace pci {

constexpr int config_area_num_dwords = 64;

constexpr uint16_t config_address_port = 0xCF8;
constexpr uint16_t config_data_port    = 0xCFC;

device_address::device_address(uint8_t bus, uint8_t slot, uint8_t func) : address_((bus << 16) | (slot << 11) | (func << 8)) {
    REQUIRE(slot < 32);
    REQUIRE(func < 8);
}

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

static_assert(sizeof(config_area) == config_area_num_dwords * 4, "");

struct vendor_desc {
    vendor      vendor_id;
    const char* text;
};
const vendor_desc known_vendors[] = {
    { vendor::lsi     , "LSI Logic / Symbios Logic" },
    { vendor::ensoniq , "Ensoniq"                   },
    { vendor::vmware  , "VMware Inc"                },
    { vendor::intel   , "Intel Corporation"         },
};

struct device_desc {
    vendor       vendor_id;
    uint16_t     device_id;
    device_class dev_class;
    const char* text;
};

const device_desc known_devices[] = {
    { vendor::lsi     , 0x0030 , device_class::scsi_controller              , "53c1030 PCI-X Fusion-MPT Dual Ultra320 SCSI"   },
    { vendor::ensoniq , 0x1371 , device_class::bidi_audio_device            , "ES1371 / Creative Labs CT2518/ES1373"          },
    { vendor::vmware  , 0x0405 , device_class::vga_controller               , "SVGA II Adapter"                               },
    { vendor::vmware  , 0x0740 , device_class::other_system_peripheral      , "Virtual Machine Communication Interface"       },
    { vendor::vmware  , 0x0790 , device_class::pci_bridge                   , "PCI bridge"                                    },
    { vendor::vmware  , 0x07A0 , device_class::pci_bridge                   , "PCI Express Root Port"                         },
    { vendor::intel   , 0x100E , device_class::ethernet_controller          , "82540EM Gigabit Ethernet Controller"           },
    { vendor::intel   , 0x100F , device_class::ethernet_controller          , "82545EM Gigabit Ethernet Controller (Copper)"  },
    { vendor::intel   , 0x10f5 , device_class::ethernet_controller          , "82567LM Gigabit Network Connection"            },
    { vendor::intel   , 0x1237 , device_class::host_bridge                  , "440FX - 82441FX PMC [Natoma]"                  },
    { vendor::intel   , 0x2448 , device_class::pci_bridge                   , "82801 Mobile PCI Bridge"                       },
    { vendor::intel   , 0x2917 , device_class::isa_bridge                   , "ICH9M-E LPC Interface Controller"              },
    { vendor::intel   , 0x2929 , device_class::serial_ata_controller        , "82801IBM/IEM SATA Controller"                  }, // 82801IBM/IEM (ICH9M/ICH9M-E) 4 port SATA Controller [AHCI mode]
    { vendor::intel   , 0x2930 , device_class::smbus                        , "82801I (ICH9 Family) SMBus Controller"         }, 
    { vendor::intel   , 0x2934 , device_class::usb_controller               , "82801I (ICH9 Family) USB UHCI Controller #1"   },
    { vendor::intel   , 0x2935 , device_class::usb_controller               , "82801I (ICH9 Family) USB UHCI Controller #2"   },
    { vendor::intel   , 0x2936 , device_class::usb_controller               , "82801I (ICH9 Family) USB UHCI Controller #3"   },
    { vendor::intel   , 0x2937 , device_class::usb_controller               , "82801I (ICH9 Family) USB UHCI Controller #4"   },
    { vendor::intel   , 0x2938 , device_class::usb_controller               , "82801I (ICH9 Family) USB UHCI Controller #5"   },
    { vendor::intel   , 0x2939 , device_class::usb_controller               , "82801I (ICH9 Family) USB UHCI Controller #6"   },
    { vendor::intel   , 0x293a , device_class::usb_controller               , "82801I (ICH9 Family) USB2 EHCI Controller #1"  },
    { vendor::intel   , 0x293c , device_class::usb_controller               , "82801I (ICH9 Family) USB2 EHCI Controller #2"  },
    { vendor::intel   , 0x293e , device_class::audio_device                 , "82801I (ICH9 Family) HD Audio Controller"      },
    { vendor::intel   , 0x2a40 , device_class::host_bridge                  , "Mobile 4 Series Chipset Memory Controller Hub" },
    { vendor::intel   , 0x2a42 , device_class::vga_controller               , "Mobile 4 Series Chipset Integrated Graphics"   }, // Mobile 4 Series Chipset Integrated Graphics Controller
    { vendor::intel   , 0x2a43 , device_class::other_display_controller     , "Mobile 4 Series Chipset Integrated Graphics"   }, // Mobile 4 Series Chipset Integrated Graphics Controller
    { vendor::intel   , 0x2a44 , device_class::other_communications_device  , "Mobile 4 Series Chipset MEI Controller"        },
    { vendor::intel   , 0x2a46 , device_class::ide_controller               , "Mobile 4 Series Chipset PT IDER Controller"    },
    { vendor::intel   , 0x2a47 , device_class::generic_serial_controller    , "Mobile 4 Series Chipset AMT SOL Redirection"   },
    { vendor::intel   , 0x4237 , device_class::other_network_controller     , "PRO/Wireless 5100 AGN Network Connection"      }, // PRO/Wireless 5100 AGN [Shiloh] Network Connection
    { vendor::intel   , 0x7000 , device_class::isa_bridge                   , "82371SB PIIX3 ISA [Natoma/Triton II]"          },
    { vendor::intel   , 0x7010 , device_class::ide_controller               , "82371SB PIIX3 IDE [Natoma/Triton II]"          },
    { vendor::intel   , 0x7020 , device_class::usb_controller               , "82371SB PIIX3 USB [Natoma/Triton II]"          },
    { vendor::intel   , 0x7110 , device_class::isa_bridge                   , "82371AB/EB/MB PIIX4 ISA"                       },
    { vendor::intel   , 0x7111 , device_class::ide_controller               , "82371AB/EB/MB PIIX4 IDE"                       },
    { vendor::intel   , 0x7113 , device_class::other_bridge                 , "82371AB/EB/MB PIIX4 ACPI"                      },
    { vendor::intel   , 0x7190 , device_class::host_bridge                  , "440BX/ZX/DX - 82443BX/ZX/DX Host bridge"       },
    { vendor::intel   , 0x7191 , device_class::pci_bridge                   , "440BX/ZX/DX - 82443BX/ZX/DX AGP bridge"        },
};

template<typename T, size_t size, typename Pred>
const T* find_info(const T (&arr)[size], Pred p) {
    auto it = std::find_if(std::begin(arr), std::end(arr), p);
    return it == std::end(arr) ? nullptr : &*it;
}

const vendor_desc* get_vendor_info(vendor vendor_id) {
    return find_info(known_vendors, [vendor_id](const vendor_desc& vi) { return vi.vendor_id == vendor_id; });
}

const device_desc* get_device_info(const config_area& ca) {
    return find_info(known_devices, [&ca](const device_desc& di) { return ca.vendor_id == di.vendor_id && ca.device_id == di.device_id && ca.dev_class == di.dev_class; });
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
all_bars read_bars(device_address addr) {
    all_bars res={};
    int barcnt = 0;

    for (uint8_t i = 0; i < 6; ++i) {
        const uint8_t reg = 4 + i; // bar0 is at offset 4

        // Save original value
        const auto orig = read_config_dword(addr, reg);

        if (!orig) {
            continue;
        }

        res[barcnt].address = orig;
        res[barcnt].size    = 0;

        // Determine size by writing all 1s and seeing what sticks (taking care that only the lower 16 bits are valid for IO bars)
        write_config_dword(addr, reg, ~0U);
        auto bar_size = read_config_dword(addr, reg);

        // Restore original bar value back
        write_config_dword(addr, reg, orig);

        // Mask of R/O bits
        if (orig & bar_is_io_mask) {
            bar_size &= bar_io_address_mask;
        } else {
            bar_size &= bar_mem_address_mask;
        }

        // Invert bits and increment 1
        res[barcnt].size = (~bar_size) + 1;

        if (!(orig & bar_is_io_mask) && (orig & bar_mem_type_mask)) {
            REQUIRE((orig & bar_mem_type_mask) == 0x4); // 64-bit BAR
            REQUIRE(i != 5);
            ++i;
            // Consume extra BAR
            const auto extra = read_config_dword(addr, reg + 1);
            write_config_dword(addr, reg + 1, ~0U);
            const auto sticky = ~read_config_dword(addr, reg + 1);
            write_config_dword(addr, reg + 1, extra);
            // Lazy: We don't actually support 64-bit BARs
            REQUIRE(extra == 0);
            REQUIRE(sticky == 0);
        }

        barcnt++;
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
    kvector<device_info> devices_;

    void probe(device_address dev_addr) {
        const auto cfgdw = read_config_dword(dev_addr, 0);
        if ((cfgdw&0xffff) == 0xffff) {
            return;
        }
        const auto ca = read_config_area(dev_addr);
        const auto header_type = ca.header_type & header_type_device_mask;
        if (header_type == 0) {
            // 0x00 = Normal devices
            dbgout() << dev_addr << " " << as_hex(ca.dev_class) << ": ";
            if (auto vi = get_vendor_info(ca.vendor_id)) {
                dbgout() << vi->text;
            } else {
                dbgout() << "Vendor " << as_hex(ca.vendor_id);
            }
            dbgout() << " ";
            if (auto di = get_device_info(ca)) {
                dbgout() << di->text;
            } else {
                dbgout() << "Device " << as_hex(ca.device_id);
            }
            dbgout() << "\n";

            devices_.push_back({dev_addr, ca, read_bars(dev_addr)});
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

    virtual array_view<device_info> do_devices() const override {
        return array_view<device_info>{devices_.begin(), devices_.end()};
    }
};

object_buffer<manager_impl> manager_buffer;

owned_ptr<manager, destruct_deleter> init() {
    return owned_ptr<manager, destruct_deleter>{manager_buffer.construct().release()};
}

} } // namespace attos::pci

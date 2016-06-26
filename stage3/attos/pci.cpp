#include "pci.h"
#include <attos/out_stream.h>
#include <attos/cpu.h>

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

uint32_t read_config_dword(device_address dev, uint8_t reg)
{
    REQUIRE(reg < 64);
// 31	        30 - 24	    23 - 16	    15 - 11	        10 - 8	        7 - 2	        1 - 0
// Enable Bit	Reserved	Bus Number	Device Number	Function Number	Register Number	00
    __outdword(config_address_port, 0x8000'0000 | static_cast<uint32_t>(dev) | (reg << 2));
    return __indword(config_data_port);
}

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
    { vendor_lsi     , 0x0030 , 0x0100 , "53c1030 PCI-X Fusion-MPT Dual Ultra320 SCSI"  },
    { vendor_ensoniq , 0x1371 , 0x0401 , "ES1371 / Creative Labs CT2518/ES1373"         },
    { vendor_vmware  , 0x0405 , 0x0300 , "SVGA II Adapter"                              },
    { vendor_vmware  , 0x0790 , 0x0604 , "PCI bridge"                                   },
    { vendor_vmware  , 0x07A0 , 0x0604 , "PCI Express Root Port"                        },
    { vendor_intel   , 0x100F , 0x0200 , "82545EM Gigabit Ethernet Controller (Copper)" },
    { vendor_intel   , 0x1237 , 0x0600 , "440FX - 82441FX PMC [Natoma]"                 },
    { vendor_intel   , 0x7000 , 0x0601 , "82371SB PIIX3 ISA [Natoma/Triton II]"         },
    { vendor_intel   , 0x7110 , 0x0601 , "82371AB/EB/MB PIIX4 ISA"                      },
    { vendor_intel   , 0x7190 , 0x0600 , "440BX/ZX/DX - 82443BX/ZX/DX Host bridge"      },
    { vendor_intel   , 0x7191 , 0x0604 , "440BX/ZX/DX - 82443BX/ZX/DX AGP bridge"       },
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

class manager_impl : public manager {
public:
    manager_impl() {
        dbgout() << "[pci] Initializing.\n";
        for (int bus = 0; bus < 256; ++bus) {
            for (int slot = 0; slot < 32; ++slot) {
                const auto dev_adr = device_address{static_cast<uint8_t>(bus), static_cast<uint8_t>(slot), 0};
                const auto cfgdw   = read_config_dword(dev_adr, 0);
                if ((cfgdw&0xffff) == 0xffff) continue;
                auto ca = read_config_area(dev_adr);
                const char* vendor_text = "Unknown vendor";
                const char* device_text = "Unknown device";
                if (auto vi = get_vendor_info(ca.vendor)) {
                    vendor_text = vi->text;
                }
                if (auto di = get_device_info(ca)) {
                    device_text = di->text;
                }
                dbgout () << dev_adr << " " << as_hex(ca.device_class) << ": " << as_hex(ca.vendor) << ":" << as_hex(ca.device_id) << " " << vendor_text << " " << device_text << "\n";
            }
        }
    }
    ~manager_impl() {
        dbgout() << "[pci] Shutting down.\n";
    }
private:
};

object_buffer<manager_impl> manager_buffer;

owned_ptr<manager, destruct_deleter> init() {
    return owned_ptr<manager, destruct_deleter>{manager_buffer.construct().release()};
}

} } // namespace attos::pci

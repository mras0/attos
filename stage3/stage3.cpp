#include <stdint.h>
#include <intrin.h>
#include <type_traits>
#include <atomic>

#include <attos/cpu.h>
#include <attos/mem.h>
#include <attos/mm.h>
#include <attos/pe.h>
#include <attos/isr.h>
#include <attos/pci.h>
#include <attos/ata.h>
#include <attos/vga/text_screen.h>

#define assert REQUIRE // undefined yadayda
#include <attos/tree.h>

namespace attos {

namespace ps2 {
constexpr uint8_t data_port    = 0x60;
constexpr uint8_t status_port  = 0x64; // when reading
constexpr uint8_t command_port = 0x64; // when writing

constexpr uint8_t status_mask_output_full = 0x01;
constexpr uint8_t status_mask_input_fill  = 0x02;

uint8_t data() {
    return __inbyte(ps2::data_port);
}

uint8_t status() {
    return __inbyte(ps2::status_port);
}


constexpr uint8_t sc_invalid = 0;
constexpr uint8_t scan_code_set_1_size = 0x58;
constexpr uint8_t scan_code_set_1[scan_code_set_1_size] = {
    /* 0x00 */ sc_invalid,
    /* 0x01 */ '\x1b', // escape
    /* 0x02 */ '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    /* 0x0c */ '-',
    /* 0x0d */ '=',
    /* 0x0e */ '\x08', // backspace
    /* 0x0f */ '\x09', // tab
    /* 0x10 */ 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\x0A', // (enter) newline
    /* 0x1D */ sc_invalid, // left control
    /* 0x1E */ 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`',
    /* 0x2A */ sc_invalid, // left shift
    /* 0x2B */ '\\',
    /* 0x2C */ 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/',
    /* 0x36 */ sc_invalid, // right shift
    /* 0x37 */ sc_invalid, // keypad *
    /* 0x38 */ sc_invalid, // left alt
    /* 0x39 */ ' ', // space
    /* 0x3A */ sc_invalid, // caps lock
    /* 0x3B */ sc_invalid, // f1
    /* 0x3C */ sc_invalid, // f2
    /* 0x3D */ sc_invalid, // ...
    /* 0x3E */ sc_invalid,
    /* 0x3F */ sc_invalid,
    /* 0x40 */ sc_invalid,
    /* 0x41 */ sc_invalid,
    /* 0x42 */ sc_invalid,
    /* 0x43 */ sc_invalid,
    /* 0x44 */ sc_invalid, // f10
    /* 0x45 */ sc_invalid, // numlock
    /* 0x46 */ sc_invalid, // scolllock
    /* 0x47 */ '7', // keypad 7
    /* 0x48 */ '8', // keypad 8
    /* 0x49 */ '9', // keypad 9
    /* 0x4A */ '-', // keypad -
    /* 0x4B */ '4', // keypad 4
    /* 0x4C */ '5', // keypad 5
    /* 0x4D */ '6', // keypad 6
    /* 0x4E */ '+', // keypad +
    /* 0x4F */ '1', // keypad 1
    /* 0x50 */ '2', // keypad 2
    /* 0x51 */ '3', // keypad 3
    /* 0x52 */ '0', // keypad 0
    /* 0x53 */ '.', // keypad .
    /* 0x54 */ sc_invalid,
    /* 0x55 */ sc_invalid,
    /* 0x56 */ sc_invalid,
    /* 0x57 */ sc_invalid, // f11
    /* 0x58 */ 'Z', // f12
};

class controller : public singleton<controller> {
public:
    explicit controller(isr_handler& isrh) {
        reg_ = isrh.register_irq_handler(irq, [this]() { isr(); });
    }

    ~controller() {
        reg_.reset();
    }

    uint8_t get_scan_key() {
        while (!(status() & status_mask_output_full)) { // wait key
            __halt();
        }
        return data();
    }

    uint8_t read_key() {
        // Very crude translation of scan keys in Scan Code Set 1
        for (;;) {
            auto sk = get_scan_key();
            if (sk == 0xE0) {
                auto sk2 = get_scan_key();
                dbgout() << "[ps2] ignoring scan key = 0xE0 0x" << as_hex(sk2) << "\n";
                continue;
            } else if (sk == 0xE1) {
                auto sk2 = get_scan_key();
                auto sk3 = get_scan_key();
                dbgout() << "[ps2] ignoring scan key = 0xE1 0x" << as_hex(sk2) << " 0x" << as_hex(sk3) << "\n";
                continue;
            }

            // Only act on key release as they are not repeated
            const bool released = (sk & 0x80) != 0;
            if (!released) continue;
            sk &= ~0x80;

            if (sk < scan_code_set_1_size) {
                const auto k = scan_code_set_1[sk];
                if (k != sc_invalid) {
                    return k;
                }
            }
            dbgout() << "[ps2] ignoring scan key = 0x" << as_hex(sk) << "\n";
        }
    }

private:
    isr_registration_ptr reg_;
    static constexpr uint8_t irq = 1;

    void isr() {
        dbgout() << "[ps2] IRQ!\n";
    }
};

} // namespace ps2

uint8_t read_key() {
    if (ps2::controller::has_instance()) {
        return ps2::controller::instance().read_key();
    }

    for (;;) {
        while (!(ps2::status() & ps2::status_mask_output_full)) { // wait key
            _mm_pause();
        }
        const uint8_t scan_code = ps2::data();  // read key
        if (scan_code >= 0x80 && scan_code <= 0xE0) { // crude wait for release of 'normal' key
            return scan_code;
        }
    }
}

enum class smap_type : uint32_t {
    end_of_list  = 0, // last entry in list (placed by stage2)
    available    = 1, // memory, available to OS
    reserved     = 2, // reserved, not available (e.g. system ROM, memory-mapped device)
    acpi_reclaim = 3, // ACPI Reclaim Memory (usable by OS after reading ACPI tables)
    acpi_nvs     = 4, // ACPI NVS Memory (OS is required to save this memory between NVS
};

#pragma pack(push, 1)
struct smap_entry {
    uint64_t  base;
    uint64_t  length;
    smap_type type;
};
#pragma pack(pop)

owned_ptr<memory_manager, destruct_deleter> construct_mm(const smap_entry* smap, const pe::IMAGE_DOS_HEADER& image_base)
{
    // Find suitable place to construct initial memory manager
    physical_address base_addr{};
    uint64_t         base_len  = 0;

    dbgout() << "Base             Length           Type\n";
    dbgout() << "FEDCBA9876543210 FEDCBA9876543210 76543210\n";

    constexpr uint64_t min_len_megabytes = 4;
    constexpr uint64_t min_base = 1ULL << 20;
    constexpr uint64_t min_len  = min_len_megabytes << 20;
    constexpr uint64_t max_base = identity_map_length - min_len_megabytes;

    // TODO: Handle unaligned areas
    for (auto e = smap; e->type != smap_type::end_of_list; ++e) {
        dbgout() << as_hex(e->base) << ' ' << as_hex(e->length) << ' ' << as_hex(static_cast<uint32_t>(e->type));
        if (e->type == smap_type::available && e->base >= min_base && e->base <= max_base && e->length >= min_len) {
            if (!base_len) {
                // Selected this one
                base_addr = physical_address{e->base};
                base_len  = e->length;
                dbgout() << " *\n";
            } else {
                // We would have selected this one
                dbgout() << " +\n";
            }
        } else {
            dbgout() << " -\n";
        }
    }

    REQUIRE(base_len != 0);
    auto mm = mm_init(base_addr, base_len);
    // Handle identity map
    static_assert(identity_map_length == 1<<30, "");
    mm->map_memory(virtual_address(identity_map_start), identity_map_length, memory_type_rwx | memory_type::ps_1gb, physical_address{0ULL});

    // Map in kernel executable image
    const auto& nth = image_base.nt_headers();
    const auto image_phys = physical_address::from_identity_mapped_ptr(&image_base);
    const auto image_size = round_up(static_cast<uint64_t>(nth.OptionalHeader.SizeOfImage), memory_manager::page_size);
    mm->map_memory(virtual_address{nth.OptionalHeader.ImageBase}, image_size, memory_type_rwx, image_phys);

    // Switch to the new PML4
    mm->ready();
    return mm;
}

} // namespace attos

using namespace attos;

struct arguments {
    const pe::IMAGE_DOS_HEADER& image_base() const {
        return *static_cast<const pe::IMAGE_DOS_HEADER*>(image_base_);
    }
    const smap_entry* smap_entries() const {
        return static_cast<const smap_entry*>(smap_entries_);
    }

private:
    physical_address image_base_;
    physical_address smap_entries_;
};


class interrupt_timer : public singleton<interrupt_timer> {
public:
    explicit interrupt_timer(isr_handler& isrh) {
        reg_ = isrh.register_irq_handler(irq, [this]() { isr(); });
    }
    ~interrupt_timer() {
        reg_.reset();
        dbgout() << "[pit] " << pit_ticks_ << " ticks elapsed\n";
    }

private:
    std::atomic<uint64_t> pit_ticks_{0};
    isr_registration_ptr reg_;

    static constexpr uint8_t irq = 0;
    void isr() {
        ++pit_ticks_;
        ++*static_cast<uint8_t*>(physical_address{0xb8000});
    }
};

void stage3_entry(const arguments& args)
{
    // First make sure we can output debug information
    vga::text_screen ts;
    set_dbgout(ts);

    // Initialize GDT
    auto cpu = cpu_init();

    // Construct initial memory manager
    auto mm = construct_mm(args.smap_entries(), args.image_base());

    // Initialize interrupt handlers
    auto ih = isr_init();

    interrupt_timer timer{*ih}; // IRQ0 PIT

    // PS2 controller
    ps2::controller ps2c{*ih};

    // PCI
    auto pci = pci::init();

    // ATA
    ata::test();

    dbgout() << "Main about done! Press any key to exit.\n";
    for (int i = 0; i < 10; ++i) {
        auto k = read_key();
        dbgout() << "Key 0x" << as_hex(k) << " - " << (int)k << " " << (char)k << "\n";
    }
}

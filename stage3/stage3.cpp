#include <stdint.h>
#include <intrin.h>
#include <type_traits>

#include <attos/util.h>
#include <attos/mem.h>
#include <attos/mm.h>
#include <attos/pe.h>
#include <attos/vga/text_screen.h>

#define assert REQUIRE // undefined yadayda
#include <attos/tree.h>

using namespace attos;

uint8_t read_key() {
    static constexpr uint8_t ps2_data_port    = 0x60;
    static constexpr uint8_t ps2_status_port  = 0x64; // when reading
    static constexpr uint8_t ps2_command_port = 0x64; // when writing
    static constexpr uint8_t ps2s_output_full = 0x01;
    static constexpr uint8_t ps2s_input_fill  = 0x02;

    while (!(__inbyte(ps2_status_port) & ps2s_output_full)) { // wait key
        __nop();
    }
    return __inbyte(ps2_data_port);  // read key
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

owned_ptr<memory_manager, destruct_deleter> construct_mm(const smap_entry* smap)
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

    return mm;
}

inline const uint64_t* table_entry(uint64_t table_value)
{
    return static_cast<const uint64_t*>(physical_address{table_value & ~table_mask});
}

void print_page_tables(physical_address cr3)
{
    dbgout() << "cr3 = " << as_hex(cr3) << "\n";
    auto pml4 = table_entry(cr3);
    for (int i = 0; i < 512; ++i ) {
        if (pml4[i] & PAGEF_PRESENT) {
            dbgout() << as_hex(i) << " " << as_hex(pml4[i]) << "\n";
            auto pdpt = table_entry(pml4[i]);
            for (int j = 0; j < 512; ++j) {
                if (pdpt[j] & PAGEF_PRESENT) {
                    dbgout() << " " << as_hex(j) << " " << as_hex(pdpt[j]) << "\n";
                    if (!(pdpt[j] & PAGEF_PAGESIZE)) {
                        auto pdt = table_entry(pdpt[j]);
                        for (int k = 0; k < 512; ++k) {
                            if (pdt[k] & PAGEF_PRESENT) {
                                dbgout() << "  " << as_hex(k) << " " << as_hex(pdt[k]) << "\n";
                                if (!(pdt[k] & PAGEF_PAGESIZE)) {
                                    auto pt = table_entry(pdt[k]);
                                    for (int l = 0; l < 512; ++l) {
                                        if (pt[l] & PAGEF_PRESENT) {
                                            dbgout() << "   " << as_hex(l) << " " << as_hex(pt[l]) << "\n";
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

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

void map_pe(memory_manager& mm, const pe::IMAGE_DOS_HEADER& image_base)
{
    const auto& nth = image_base.nt_headers();
    mm.map_memory(virtual_address{nth.OptionalHeader.ImageBase}, memory_manager::page_size, memory_type_rwx, physical_address{&image_base});
    for (const auto& s : nth.sections()) {
#if 0
        for (auto c: s.Name) {
            dbgout() << char(c ? c : ' ');
        }
        dbgout() << " " << as_hex(s.VirtualAddress + nth.OptionalHeader.ImageBase) << " " << as_hex(s.Misc.VirtualSize) << "\n";
#endif
        REQUIRE(s.PointerToRawData); // real bss not implemented....

        const auto virt = virtual_address{(s.VirtualAddress + nth.OptionalHeader.ImageBase) & ~(memory_manager::page_size-1)};
        const auto size = round_up(static_cast<uint64_t>(s.Misc.VirtualSize), memory_manager::page_size);
        const uint8_t* data = &image_base.rva<uint8_t>(s.PointerToRawData);
        mm.map_memory(virt, size, memory_type_rwx, physical_address{data});
    }
}

void stage3_entry(const arguments& args)
{
    // First make sure we can output debug information
    vga::text_screen ts;
    set_dbgout(ts);

    // Construct initial memory manager
    auto mm = construct_mm(args.smap_entries());
    // Map in the stage3 executable
    map_pe(*mm, args.image_base());
    // Switch to the new PML4
    mm_ready();

    dbgout() << "Press any key to exit.\n";
    read_key();
}

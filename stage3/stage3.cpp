#include <stdint.h>
#include <intrin.h>
#include <type_traits>

#include <attos/mem.h>
#include <attos/pe.h>
#include <attos/vga/text_screen.h>


#define REQUIRE(expr) do { if (!(expr)) { ::attos::dbgout() << #expr << " failed in " << __FILE__ << ":" << __LINE__ << " . Hanging.\n"; ::attos::halt(); } } while (0);

namespace attos {

void halt() { for (;;) __halt(); }


out_stream* global_dbgout;

void set_dbgout(out_stream& os) {
    global_dbgout = &os;
}

out_stream& dbgout() {
    return *global_dbgout;
}

}  // namespace attos

using namespace attos;

const unsigned char bochs_magic_code[] = { 0x66, 0x87, 0xDB, 0xC3 }; // xchg bx, bx; ret
auto bochs_magic = ((void (*)(void))(void*)bochs_magic_code);

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

struct arguments {
    const pe::IMAGE_DOS_HEADER&  image_base;
    smap_entry*                  smap_entries;
};

void print_page_tables(uint64_t cr3)
{
    dbgout() << "cr3 = " << as_hex(cr3) << "\n";
    auto pml4 = (uint64_t*)cr3;
    for (int i = 0; i < 512; ++i ) {
        if (pml4[i] & PAGEF_PRESENT) {
            dbgout() << as_hex(i) << " " << as_hex(pml4[i]) << "\n";
            auto pdpt = (uint64_t*)(pml4[i]&~511);
            for (int j = 0; j < 512; ++j) {
                if (pdpt[j] & PAGEF_PRESENT) {
                    dbgout() << " " << as_hex(j) << " " << as_hex(pdpt[j]) << "\n";
                    if (!(pdpt[j] & PAGEF_PAGESIZE)) {
                        auto pdt = (uint64_t*)(pdpt[j]&~511);
                        for (int k = 0; k < 512; ++k) {
                            if (pdt[k] & PAGEF_PRESENT) {
                                dbgout() << "  " << as_hex(k) << " " << as_hex(pdt[k]) << "\n";
                                if (!(pdt[k] & PAGEF_PAGESIZE)) {
                                    auto pt = (uint64_t*)(pdt[k]&~511);
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

class boostrap_memory_manager {
public:
    explicit boostrap_memory_manager(uint64_t physical_base, uint64_t length) : base_(physical_address<uint8_t>(physical_base)), end_(base_ + length), cur_(base_) {
        REQUIRE(length != 0);
        REQUIRE(!(physical_base & (min_align_-1)));

        dbgout() << "[bootmm] Starting. Base 0x" << as_hex(physical_base) << " Length " << (length>>20) << " MB\n";
    }
    boostrap_memory_manager(const boostrap_memory_manager&) = delete;
    boostrap_memory_manager& operator=(const boostrap_memory_manager&) = delete;

    uint8_t* alloc(uint64_t size) {
        REQUIRE(cur_ + size <= end_);
        auto ptr = cur_;
        if (auto excess = size % min_align_) {
            // Round up allocation
            size += min_align_ - excess;
        }
        cur_ += size;
        __stosq(reinterpret_cast<uint64_t*>(ptr), 0, size / 8);
        return ptr;
    }


private:
    static constexpr uint64_t min_align_ = 4096;
    uint8_t* const            base_;
    uint8_t* const            end_;
    uint8_t*                  cur_;
};
object_buffer<boostrap_memory_manager> boot_mm_buffer;

auto construct_boot_mm(const smap_entry* smap)
{
    // Find suitable place to construct initial memory manager
    uint64_t base_addr = 0;
    uint64_t base_len  = 0;

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
                base_addr = e->base;
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
    return boot_mm_buffer.construct(base_addr, base_len);
}

void small_exe(const arguments& args)
{
    vga::text_screen ts;
    set_dbgout(ts);

    auto boot_mm = construct_boot_mm(args.smap_entries);
    auto buf = boot_mm->alloc(8192);
    buf[0] = 'X';
    buf[1] = '\0';
    dbgout() << (char*)buf << "\n";
    dbgout() << "Press any key\n";
    read_key();

    print_page_tables(__readcr3());

    const auto& nth = args.image_base.nt_headers();
    for (const auto& s : nth.sections()) {
        for (auto c: s.Name) {
            dbgout() << char(c ? c : ' ');
        }
        dbgout() << " " << as_hex(s.VirtualAddress + nth.OptionalHeader.ImageBase) << " " << as_hex(s.Misc.VirtualSize) << "\n";
    }

    dbgout() << "Press any key to exit.\n";
    read_key();
}

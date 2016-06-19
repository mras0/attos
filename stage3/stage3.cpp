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
#define ENUM_BIT_OP(type, op, inttype) \
constexpr inline type operator op(type l, type r) { return static_cast<type>(static_cast<inttype>(l) op static_cast<inttype>(r)); }
#define ENUM_BIT_OPS(type, inttype) \
    ENUM_BIT_OP(type, |, inttype)

enum class memory_type : uint32_t {
    read    = 0x01,
    write   = 0x02,
    execute = 0x04,
};

ENUM_BIT_OPS(memory_type, uint32_t)
static constexpr auto memory_type_rwx = memory_type::read | memory_type::write | memory_type::execute;

class virtual_address {
public:
    constexpr explicit virtual_address(uint64_t addr=0) : addr_(addr) {
    }

    constexpr operator uint64_t() const { return addr_; }

    constexpr uint32_t pml4e() const { return (addr_ >> pml4_shift) & ~511; }
    constexpr uint32_t pdpe()  const { return (addr_ >> pdp_shift)  & ~511; }
    constexpr uint32_t pde()   const { return (addr_ >> pd_shift)   & ~511; }
    constexpr uint32_t pte()   const { return (addr_ >> pt_shift)   & ~511; }

private:
    uint64_t addr_;
};

class memory_mapping {
public:
    explicit memory_mapping(virtual_address addr, uint64_t length, memory_type type) : addr_(addr), length_(length), type_(type) {
        REQUIRE(length != 0);
    }
    memory_mapping(const memory_mapping&) = delete;
    memory_mapping& operator=(const memory_mapping&) = delete;

    virtual_address address() const { return addr_; }
    uint64_t        length()  const { return length_; }
    memory_type     type()    const { return type_; }

private:
    virtual_address addr_;
    uint64_t        length_;
    memory_type     type_;
};

template<typename T>
constexpr auto round_up(T val, T align)
{
    return val % align ? val + align - (val % align) : val;
}

template<uint64_t Alignment>
class simple_heap {
public:
    explicit simple_heap(uint8_t* base, uint64_t length) : base_(base), end_(base + length), cur_(base) {
        REQUIRE(!((uint64_t)base & (Alignment-1)));
        REQUIRE(length >= Alignment);
    }
    simple_heap(const simple_heap&) = delete;
    simple_heap& operator=(const simple_heap&) = delete;

    uint8_t* alloc(uint64_t size) {
        size = round_up(size, Alignment);
        REQUIRE(cur_ + size <= end_);
        auto ptr = cur_;
        cur_ += size;
        return ptr;
    }

private:
    uint8_t* const base_;
    uint8_t* const end_;
    uint8_t* cur_;
};

template<typename T>
class fixed_size_object_heap {
public:
    explicit fixed_size_object_heap(uint8_t* base, uint64_t length) : heap_{base, length} {
    }

    template<typename... Args>
    T* construct(Args&&... args) {
        return new (heap_.alloc(sizeof(T))) T(static_cast<Args&&>(args)...);
    }

private:
    simple_heap<alignof(T)> heap_;
};

class boostrap_memory_manager {
public:
    static constexpr uint64_t page_size    = 4096;

    explicit boostrap_memory_manager(uint64_t physical_base, uint64_t length)
        : physical_pages_{physical_address<uint8_t>(physical_base), length}
        , memory_mappings_{alloc_physical(page_size), page_size} {
        dbgout() << "[bootmm] Starting. Base 0x" << as_hex(physical_base) << " Length " << (length>>20) << " MB\n";
    }
    boostrap_memory_manager(const boostrap_memory_manager&) = delete;
    boostrap_memory_manager& operator=(const boostrap_memory_manager&) = delete;

    uint8_t* alloc_physical(uint64_t size) {
        size = round_up(size, page_size);
        auto ptr = physical_pages_.alloc(size);
        __stosq(reinterpret_cast<uint64_t*>(ptr), 0, size / 8);
        return ptr;
    }

    void alloc_virtual(virtual_address base, uint64_t length, memory_type type) {
        dbgout() << "[bootmm] alloc_virtual " << as_hex(base) << " " << as_hex(length) << " type=0x" << as_hex((uint32_t)type) << "\n";

        auto mm = memory_mappings_.construct(base, length, type);
        (void)mm;
        //REQUIRE(!"Not implemented");
        dbgout() << "Not implemented further.\n";
    }

private:
    simple_heap<page_size>                 physical_pages_;
    fixed_size_object_heap<memory_mapping> memory_mappings_;
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
    auto boot_mm = boot_mm_buffer.construct(base_addr, base_len);
    // Handle identity map
    boot_mm->alloc_virtual(virtual_address(identity_map_start), identity_map_length, memory_type_rwx);

    return boot_mm;
}

void small_exe(const arguments& args)
{
    vga::text_screen ts;
    set_dbgout(ts);

    auto boot_mm = construct_boot_mm(args.smap_entries);
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

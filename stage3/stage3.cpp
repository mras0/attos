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

#define assert REQUIRE // undefined yadayda
#include <attos/tree.h>

using namespace attos;

constexpr uint8_t bochs_magic_code[] = { 0x66, 0x87, 0xDB, 0xC3 }; // xchg bx, bx; ret
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

#define ENUM_BIT_OP(type, op, inttype) \
constexpr inline type operator op(type l, type r) { return static_cast<type>(static_cast<inttype>(l) op static_cast<inttype>(r)); }
#define ENUM_BIT_OPS(type, inttype) \
    ENUM_BIT_OP(type, |, inttype)   \
    ENUM_BIT_OP(type, &, inttype)

enum class memory_type : uint32_t {
    read    = 0x01,
    write   = 0x02,
    execute = 0x04,

    //ps_2mb  = 0x1000,
    ps_1gb  = 0x2000,
};

ENUM_BIT_OPS(memory_type, uint32_t)
constexpr auto memory_type_rwx = memory_type::read | memory_type::write | memory_type::execute;

template<typename T>
class address_base {
public:
    constexpr explicit address_base(uint64_t addr=0) : addr_(addr) {
    }

    constexpr operator uint64_t() const { return addr_; }

    address_base& operator+=(uint64_t rhs) {
        addr_ += rhs;
        return *this;
    }

protected:
    uint64_t addr_;
};

class virtual_address : public address_base<virtual_address> {
public:
    constexpr explicit virtual_address(uint64_t addr=0) : address_base(addr) {
    }

    constexpr uint32_t pml4e() const { return (addr_ >> pml4_shift) & 511; }
    constexpr uint32_t pdpe()  const { return (addr_ >> pdp_shift)  & 511; }
    constexpr uint32_t pde()   const { return (addr_ >> pd_shift)   & 511; }
    constexpr uint32_t pte()   const { return (addr_ >> pt_shift)   & 511; }
};

class physical_address : public address_base<physical_address> {
public:
    constexpr explicit physical_address(uint64_t addr=0) : address_base(addr) {
    }

    constexpr explicit physical_address(const void* ptr) : address_base(reinterpret_cast<uint64_t>(ptr) - identity_map_start) {
    }

    template<typename T>
    constexpr operator T*() const { return reinterpret_cast<T*>(addr_ + identity_map_start); }
};

class memory_mapping {
private:
    virtual_address addr_;
    uint64_t        length_;
    memory_type     type_;
    tree_node       link_;

public:
    explicit memory_mapping(virtual_address addr, uint64_t length, memory_type type) : addr_(addr), length_(length), type_(type), link_() {
        REQUIRE(length != 0);
    }
    memory_mapping(const memory_mapping&) = delete;
    memory_mapping& operator=(const memory_mapping&) = delete;

    virtual_address address() const { return addr_; }
    uint64_t        length()  const { return length_; }
    memory_type     type()    const { return type_; }

    struct compare {
        bool operator()(const memory_mapping& l, const memory_mapping& r) const {
            return l.addr_ < r.addr_;
        }
    };
    using tree_type = tree<memory_mapping, &memory_mapping::link_, compare>;
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


auto find_mapping(memory_mapping::tree_type& t, virtual_address addr, uint64_t length)
{
    //auto it = memory_map_tree_.lower_bound(memory_mapping{addr, length, memory_type_rwx});
    //or something smarter
    return std::find_if(t.begin(), t.end(), [addr, length](const auto& m) { return memory_areas_overlap(addr, length, m.address(), m.length()); });
}

class boostrap_memory_manager {
public:
    static constexpr uint64_t page_size = 4096;

    explicit boostrap_memory_manager(physical_address base, uint64_t length)
        : physical_pages_{base, length}
        , memory_mappings_{alloc_physical(page_size), page_size}
        , saved_cr3_(__readcr3()) {
        dbgout() << "[bootmm] Starting. Base 0x" << as_hex(base) << " Length " << (length>>20) << " MB\n";

        pml4_ = static_cast<uint64_t*>(alloc_physical(page_size));
    }

    ~boostrap_memory_manager() {
        dbgout() << "[bootmm] Shutting down. Restoring CR3 to " << as_hex(saved_cr3_) << "\n";
        __writecr3(saved_cr3_);
    }

    boostrap_memory_manager(const boostrap_memory_manager&) = delete;
    boostrap_memory_manager& operator=(const boostrap_memory_manager&) = delete;

    physical_address saved_cr3() const {
        return saved_cr3_;
    }

    physical_address pml4() const {
        return physical_address{pml4_};
    }

    physical_address alloc_physical(uint64_t size) {
        size = round_up(size, page_size);
        auto ptr = physical_pages_.alloc(size);
        __stosq(reinterpret_cast<uint64_t*>(ptr), 0, size / 8);
        return physical_address{ptr};
    }

    void map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) {
        dbgout() << "[bootmm] map_memory virt=" << as_hex(virt) << " " << as_hex(length) << " type=0x" << as_hex((uint32_t)type) << " phys=" << as_hex(phys) << "\n";

        const uint64_t map_page_size = static_cast<uint32_t>(type & memory_type::ps_1gb) ? (1<<30) : (1<<12);

        // Check address alignment
        REQUIRE((virt & (map_page_size - 1)) == 0);
        REQUIRE((phys & (map_page_size - 1)) == 0);

        // Check length
        REQUIRE(length > 0);
        REQUIRE(virt + length > virt && "No wraparound allowed");
        REQUIRE((length & (map_page_size - 1)) == 0);

        auto it = find_mapping(memory_map_tree_, virt, length);
        if (it != memory_map_tree_.end()) {
            dbgout() << "[bootmm] FATAL ERROR overlaps " << as_hex(it->address()) << "\n";
            REQUIRE(false);
        }

        const uint64_t flags = PAGEF_WRITE;

        auto mm = memory_mappings_.construct(virt, length, type);
        memory_map_tree_.insert(*mm);

        for (; length; length -= map_page_size, virt += map_page_size, phys += map_page_size) {
            auto* pdp = alloc_if_not_present(pml4_[virt.pml4e()], flags);

            if (static_cast<uint32_t>(type & memory_type::ps_1gb)) {
                pdp[virt.pdpe()] = phys | PAGEF_PAGESIZE | PAGEF_PRESENT | flags;
            } else {
                auto* pd = alloc_if_not_present(pdp[virt.pdpe()], flags);
                auto* pt = alloc_if_not_present(pd[virt.pde()], flags);
                pt[virt.pte()] = phys | PAGEF_PRESENT | flags;
            }
        }
    }

private:
    simple_heap<page_size>                 physical_pages_;
    fixed_size_object_heap<memory_mapping> memory_mappings_;
    memory_mapping::tree_type              memory_map_tree_;
    physical_address                       saved_cr3_;
    uint64_t*                              pml4_;

    uint64_t* alloc_if_not_present(uint64_t& parent, uint64_t flags) {
        if (parent & PAGEF_PRESENT) {
            // TODO: Check flags
            return reinterpret_cast<uint64_t*>(parent & ~(page_size -1));
        }
        return alloc_table_entry(parent, flags);
    }

    uint64_t* alloc_table_entry(uint64_t& parent, uint64_t flags) {
        auto table = static_cast<uint64_t*>(alloc_physical(page_size));
        parent = physical_address{table} | PAGEF_PRESENT | flags;
        dbgout() << "[bootmm] Allocated page table. parent " << as_hex((uint64_t)&parent) << " <- " << as_hex(parent) << "\n";
        return table;
    }
};
object_buffer<boostrap_memory_manager> boot_mm_buffer;

auto construct_boot_mm(const smap_entry* smap)
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
    auto boot_mm = boot_mm_buffer.construct(base_addr, base_len);
    // Handle identity map
    static_assert(identity_map_length == 1<<30, "");
    boot_mm->map_memory(virtual_address(identity_map_start), identity_map_length, memory_type_rwx | memory_type::ps_1gb, physical_address{0ULL});

    return boot_mm;
}

inline const uint64_t* table_entry(uint64_t table_value)
{
    return static_cast<const uint64_t*>(physical_address{table_value & ~511});
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

void map_pe(boostrap_memory_manager& boot_mm, const pe::IMAGE_DOS_HEADER& image_base)
{
    const auto& nth = image_base.nt_headers();
    boot_mm.map_memory(virtual_address{nth.OptionalHeader.ImageBase}, boostrap_memory_manager::page_size, memory_type_rwx, physical_address{&image_base});
    for (const auto& s : nth.sections()) {
#if 0
        for (auto c: s.Name) {
            dbgout() << char(c ? c : ' ');
        }
        dbgout() << " " << as_hex(s.VirtualAddress + nth.OptionalHeader.ImageBase) << " " << as_hex(s.Misc.VirtualSize) << "\n";
#endif
        REQUIRE(s.PointerToRawData); // real bss not implemented....

        const auto virt = virtual_address{(s.VirtualAddress + nth.OptionalHeader.ImageBase) & ~(boostrap_memory_manager::page_size-1)};
        const auto size = round_up(static_cast<uint64_t>(s.Misc.VirtualSize), boostrap_memory_manager::page_size);
        const uint8_t* data = &image_base.rva<uint8_t>(s.PointerToRawData);
        boot_mm.map_memory(virt, size, memory_type_rwx, physical_address{data});
    }
}

void stage3_entry(const arguments& args)
{
    // First make sure we can output debug information
    vga::text_screen ts;
    set_dbgout(ts);

    // Construct initial memory manager
    auto boot_mm = construct_boot_mm(args.smap_entries());
    // Map in the stage3 executable
    map_pe(*boot_mm, args.image_base());
    // Allocate a proper stack
    const auto& image_oh = args.image_base().nt_headers().OptionalHeader;
    const auto initial_stack_size = image_oh.SizeOfStackCommit;
    auto stack_ptr = boot_mm->alloc_physical(initial_stack_size);
    boot_mm->map_memory(virtual_address{image_oh.ImageBase - initial_stack_size}, initial_stack_size, memory_type::read | memory_type::write, stack_ptr);
    // TODO: set stack pointer and restore it again later

    print_page_tables(boot_mm->saved_cr3());

    const auto new_cr3 = boot_mm->pml4();
    dbgout() << "Setting CR3 to " << as_hex(new_cr3) << "\n";
    bochs_magic();
    __writecr3(new_cr3);
    print_page_tables(new_cr3);

    dbgout() << "Press any key to exit.\n";
    read_key();
}

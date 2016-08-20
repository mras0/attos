#include "mm.h"
#include <attos/cpu.h>
#include <attos/out_stream.h>

#define assert(expr)
#include <attos/tree.h>
#undef assert

namespace attos {

inline uint64_t* table_entry(uint64_t table_value) {
    return static_cast<uint64_t*>(physical_address{table_value & ~(PAGEF_NX | (memory_manager::page_size - 1))});
}

inline uint64_t* present_table_entry(uint64_t table_value) {
    REQUIRE(table_value & PAGEF_PRESENT);
    return table_entry(table_value);
}

template<uint64_t Alignment>
class no_free_heap {
public:
    explicit no_free_heap(uint8_t* base, uint64_t length) : base_(base), end_(base + length), cur_(base) {
        REQUIRE(!((uint64_t)base & (Alignment-1)));
        REQUIRE(length >= Alignment);
    }
    no_free_heap(const no_free_heap&) = delete;
    no_free_heap& operator=(const no_free_heap&) = delete;

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
    no_free_heap<alignof(T)> heap_;
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

auto find_mapping(memory_mapping::tree_type& t, virtual_address addr, uint64_t length) {
    //auto it = memory_map_tree_.lower_bound(memory_mapping{addr, length, memory_type_rwx});
    //or something smarter
    return std::find_if(t.begin(), t.end(), [addr, length](const auto& m) { return memory_areas_overlap(addr, length, m.address(), m.length()); });
}

void free_physical_page(physical_address addr);

constexpr virtual_address kernel_map_start{0xFFFFFFFF'FF000000};
constexpr uint32_t        kernel_pml4 = 0x1ff;

class memory_manager_base : public memory_manager {
public:
    explicit memory_manager_base()
        : memory_mappings_phys_{alloc_physical(page_size)}
        , memory_mappings_{memory_mappings_phys_.address(), memory_mappings_phys_.length()}
        , pml4_{alloc_table()}
        , free_virt_base_{kernel_map_start-(1ULL<<30)} { // HACK: ISR needs virtual memory within 2GB of the kernel code...
    }

    ~memory_manager_base() {
        for (uint32_t l4 = 0; l4 < table_size; ++l4) {
            if (!(pml4_[l4] & PAGEF_PRESENT)) continue;
            auto pdpt = table_entry(pml4_[l4]);
            for (uint32_t l3 = 0; l3 < table_size; ++l3) {
                if (!(pdpt[l3] & PAGEF_PRESENT)) continue;
                if (pdpt[l3] & PAGEF_PAGESIZE) continue;
                auto pdt = table_entry(pdpt[l3]);
                for (uint32_t l2 = 0; l2 < table_size; ++l2) {
                    if (!(pdt[l2] & PAGEF_PRESENT)) continue;
                    if ((pdt[l2] & PAGEF_PAGESIZE)) continue;
                    auto pt = table_entry(pdt[l2]);
                    free_table(pt);
                }
                free_table(pdt);
            }
            free_table(pdpt);
        }
        //REQUIRE(std::find_if(pml4_, pml4_ + table_size, [](uint64_t e) { return e != 0; }) == pml4_ + table_size);
        free_table(pml4_);
    }

    memory_manager_base(const memory_manager_base&) = delete;
    memory_manager_base& operator=(const memory_manager_base&) = delete;

    static constexpr uint32_t table_size = 512;

    physical_address pml4() const {
        return physical_address::from_identity_mapped_ptr(pml4_);
    }

private:
    physical_allocation                    memory_mappings_phys_;
    fixed_size_object_heap<memory_mapping> memory_mappings_;
    memory_mapping::tree_type              memory_map_tree_;
    uint64_t*                              pml4_;
    virtual_address                        free_virt_base_;

    static uint64_t* alloc_table() {
        return static_cast<uint64_t*>(alloc_physical(memory_manager::page_size).release());
    }

    static void free_table(uint64_t* table) {
        free_physical_page(physical_address::from_identity_mapped_ptr(table));
    }

    uint64_t* alloc_if_not_present(uint64_t& parent, uint64_t flags) {
        if (parent & PAGEF_PRESENT) {
            constexpr uint64_t check_mask = (PAGEF_NX | 0xFFF) & ~(PAGEF_ACCESSED | PAGEF_DIRTY);
            if ((parent & check_mask) != (flags & check_mask)) {
                dbgout() << "parent = " << as_hex(parent) << "\n";
                dbgout() << "flags  = " << as_hex(flags) << "\n";
                FATAL_ERROR("Page flags incompatible");
            }
            return table_entry(parent);
        }
        return alloc_table_entry(parent, flags);
    }

    uint64_t* alloc_table_entry(uint64_t& parent, uint64_t flags) {
        auto table = static_cast<uint64_t*>(alloc_table());
        parent = physical_address::from_identity_mapped_ptr(table) | flags;
        //dbgout() << "[mem] Allocated page table. parent " << as_hex((uint64_t)&parent) << " <- " << as_hex(parent) << "\n";
        return table;
    }

    virtual void do_switch_to() override {
        __writecr3(pml4());
    }

    virtual_address virtual_alloc(uint64_t length) {
        // TODO: Improve this...
        REQUIRE((length & (page_size-1)) == 0);
        const auto addr = free_virt_base_;
        free_virt_base_ += length;
        REQUIRE(free_virt_base_ <= kernel_map_start);
        REQUIRE(free_virt_base_ >= addr); // Make sure we didn't wrap around
        return addr;
    }

protected:
    virtual virtual_address do_map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) override {
        const uint64_t map_page_size = memory_type_page_size(type);

        // Check length
        REQUIRE(length > 0);
        REQUIRE((length & (map_page_size - 1)) == 0);

        // Check physical address alignment
        REQUIRE((phys & (map_page_size - 1)) == 0);
        REQUIRE(phys < 1ULL<<32); // Probably more work required before we support > 4GB addresses

        // Alloc virtual address (if needed)
        if (virt == memory_manager::map_alloc_virt) {
            virt = virtual_alloc(length);
        } else if (virt == memory_manager::map_alloc_virt_close_to_kernel) {
            virt = virtual_alloc(length);
        }

        dbgout() << "[mem] map " << as_hex(virt) << " <- " << as_hex(phys) << " len " << as_hex(length).width(0) << ' ' << type << "\n";

        // Check virtual address
        REQUIRE((virt & (map_page_size - 1)) == 0);
        REQUIRE(virt + length > virt && "No wraparound allowed");

        const auto virt_start = virt;

        auto it = find_mapping(memory_map_tree_, virt, length);
        if (it != memory_map_tree_.end()) {
            dbgout() << "[mem] FATAL ERROR overlaps " << as_hex(it->address()) << "\n";
            REQUIRE(false);
        }

        // Flags
        REQUIRE(static_cast<uint32_t>(type & memory_type::read));

        const uint64_t flags  = PAGEF_PRESENT
            | (static_cast<uint32_t>(type & memory_type::user) ? PAGEF_USER : 0);
        const auto page_flags = flags
            | (static_cast<uint32_t>(type & memory_type::write) ? PAGEF_WRITE : 0)
            | (static_cast<uint32_t>(type & memory_type::execute) ? 0 : PAGEF_NX)
            | (static_cast<uint32_t>(type & memory_type::cache_disable) ? PAGEF_PWT | PAGEF_PCD : 0);

        auto mm = memory_mappings_.construct(virt, length, type);
        memory_map_tree_.insert(*mm);

        for (; length; length -= map_page_size, virt += map_page_size, phys += map_page_size) {
            auto* pdp = alloc_if_not_present(pml4_[virt.pml4e()], flags | PAGEF_WRITE);

            if (static_cast<uint32_t>(type & memory_type::ps_1gb)) {
                pdp[virt.pdpe()] = phys | PAGEF_PAGESIZE | page_flags;
            } else {
                auto* pd = alloc_if_not_present(pdp[virt.pdpe()], flags | PAGEF_WRITE);
                if (static_cast<uint32_t>(type & memory_type::ps_2mb)) {
                    pd[virt.pde()] = phys | PAGEF_PAGESIZE | page_flags;
                } else {
                    auto* pt = alloc_if_not_present(pd[virt.pde()], flags | PAGEF_WRITE);
                    pt[virt.pte()] = phys | page_flags;
                }
            }
            //dbgout() << "[mem] " << as_hex(virt) << " " << as_hex(phys) << " " << as_hex(page_flags) << "\n";
        }

        return virt_start;
    }

    virtual void do_unmap_memory(virtual_address virt, uint64_t length) override {
        auto it = find_mapping(memory_map_tree_, virt, length);
        REQUIRE(it != memory_map_tree_.end());
        const auto type = it->type();
        const uint64_t map_page_size = memory_type_page_size(it->type());
        REQUIRE(it->length() == length);

        dbgout() << "[mem] unmap " << as_hex(virt) << " len " << as_hex(length).width(0) << ' ' << it->type() << "\n";

        memory_map_tree_.remove(*it);
        for (; length; length -= map_page_size, virt += map_page_size) {
            auto* pdp = present_table_entry(pml4_[virt.pml4e()]);

            if (static_cast<uint32_t>(type & memory_type::ps_1gb)) {
                pdp[virt.pdpe()] = 0;
            } else {
                auto* pd = present_table_entry(pdp[virt.pdpe()]);
                if (static_cast<uint32_t>(type & memory_type::ps_2mb)) {
                    pd[virt.pde()] = 0;
                } else {
                    auto* pt = present_table_entry(pd[virt.pde()]);
                    pt[virt.pte()] = 0;
                }
            }
        }
        // TODO: Free empty tables
        memory_map_tree_.remove(*it);
        // TODO: The memory_mapping in *it can now be reused
    }

};

constexpr uint64_t initial_heap_size = 1<<20;

class kernel_memory_manager : public memory_manager, public singleton<kernel_memory_manager> {
public:
    explicit kernel_memory_manager(physical_address base, uint64_t length)
        : saved_cr3_{__readcr3()}
        , physical_pages_{base, length}
        , kernel_heap_phys_{alloc_physical(initial_heap_size)}
        , kernel_heap_{static_cast<uint8_t*>(kernel_heap_phys_.address()), kernel_heap_phys_.length()} {
        dbgout() << "[mem] Starting. Base 0x" << as_hex(base) << " Length " << (length>>20) << " MB\n";
        mm_ = mm_buffer_.construct();
    }

    ~kernel_memory_manager() {
        dbgout() << "[mem] Shutting down. Restoring CR3 to " << as_hex(saved_cr3_) << "\n";
        __writecr3(saved_cr3_);
    }

    kernel_memory_manager(const kernel_memory_manager&) = delete;
    kernel_memory_manager& operator=(const kernel_memory_manager&) = delete;

    physical_allocation alloc_physical(uint64_t size) {
        size = round_up(size, page_size);
        auto ptr = physical_pages_.alloc(size);
        __stosq(reinterpret_cast<uint64_t*>(ptr), 0, size / 8);
        return { physical_address::from_identity_mapped_ptr(ptr), size };
    }

    void free_physical(physical_address addr, uint64_t length) {
        physical_pages_.free(addr, length);
    }

    physical_address pml4() const {
        return mm_->pml4();
    }

    void* alloc(uint64_t size) {
        return kernel_heap_.alloc(size);
    }

    void free(void* ptr) {
        return kernel_heap_.free(ptr);
    }
private:
    physical_address                                 saved_cr3_;
    simple_heap                                      physical_pages_;
    physical_allocation                              kernel_heap_phys_;
    default_heap                                     kernel_heap_;
    object_buffer<memory_manager_base>               mm_buffer_;
    owned_ptr<memory_manager_base, destruct_deleter> mm_;

    virtual void do_switch_to() override {
        return mm_->switch_to();
    }

    virtual virtual_address do_map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) override {
        REQUIRE(virt.pml4e() == kernel_pml4);
        return mm_->map_memory(virt, length, type, phys);
    }

    void do_unmap_memory(virtual_address virt, uint64_t length) {
        mm_->unmap_memory(virt, length);
    }
};
object_buffer<kernel_memory_manager> mm_buffer;

physical_allocation::~physical_allocation() {
    if (length_) {
        kernel_memory_manager::instance().free_physical(addr_, length_);
    }
}

physical_allocation& physical_allocation::operator=(physical_allocation&& other) {
    REQUIRE(length_ == 0);
    addr_ = other.addr_;
    length_ = other.length_;
    other.length_ = 0;
    return *this;
}

physical_address physical_allocation::release() {
    REQUIRE(length_ == memory_manager::page_size);
    length_ = 0;
    return addr_;
}

memory_manager_ptr mm_init(physical_address base, uint64_t length)
{
    auto mm = mm_buffer.construct(base, length);
    return memory_manager_ptr{mm.release()};
}

void print_page_tables(physical_address pml4_address)
{
    dbgout() << "PML4 " << as_hex(pml4_address) << "\n";

    constexpr auto table_size = memory_manager_base::table_size;
    auto pml4 = table_entry(pml4_address);
    for (uint32_t l4 = 0; l4 < table_size; ++l4) {
        const auto l4v = static_cast<uint64_t>(l4)<<virtual_address::pml4_shift;
        if (!(pml4[l4] & PAGEF_PRESENT)) continue;
        dbgout() << as_hex(l4) << " " << as_hex(pml4[l4]) << " " << as_hex(l4v) << "\n";
        auto pdpt = table_entry(pml4[l4]);
        for (uint32_t l3 = 0; l3 < table_size; ++l3) {
            const auto l3v = static_cast<uint64_t>(l3)<<virtual_address::pdp_shift;
            if (!(pdpt[l3] & PAGEF_PRESENT)) continue;
            dbgout() << " " << as_hex(l3) << " " << as_hex(pdpt[l3]) << " " << as_hex(l4v|l3v) << "\n";
            if (pdpt[l3] & PAGEF_PAGESIZE) continue;
            auto pdt = table_entry(pdpt[l3]);
            if ((pdt[0]&~(PAGEF_ACCESSED|PAGEF_DIRTY)) == (PAGEF_NX|PAGEF_PRESENT|PAGEF_PAGESIZE|PAGEF_WRITE)) {
                dbgout() << "  Identity mapping\n";
                continue;
            }
            for (uint32_t l2 = 0; l2 < table_size; ++l2) {
                const auto l2v = static_cast<uint64_t>(l2)<<virtual_address::pd_shift;
                if (!(pdt[l2] & PAGEF_PRESENT)) continue;
                dbgout() << "  " << as_hex(l2) << " " << as_hex(pdt[l2]) << " " << as_hex(l4v|l3v|l2v) << "\n";
                if ((pdt[l2] & PAGEF_PAGESIZE)) continue;
                auto pt = table_entry(pdt[l2]);
                for (uint32_t l1 = 0; l1 < memory_manager_base::table_size; ++l1) {
                    const auto l1v = static_cast<uint64_t>(l1)<<virtual_address::pt_shift;
                    if (!(pt[l1] & PAGEF_PRESENT)) continue;
                    dbgout() << "   " << as_hex(l1) << " " << as_hex(pt[l1]) << " " << as_hex(l4v|l3v|l2v|l1v) << "\n";
                }
            }
        }
    }
}

physical_address virt_to_phys(physical_address pml4, virtual_address virt)
{
    //dbgout() << "virt_to_phys(" << as_hex(pml4) << ", " << as_hex(virt) << ")\n";

    const auto pml4e = table_entry(pml4)[virt.pml4e()];
    //dbgout() << "PML4 " << as_hex(pml4e) << "\n";
    REQUIRE(pml4e & PAGEF_PRESENT);

    const auto pdpe  = table_entry(pml4e)[virt.pdpe()];
    //dbgout() << "PDP  " << as_hex(pdpe) << "\n";
    REQUIRE(pdpe & PAGEF_PRESENT);
    if (pdpe & PAGEF_PAGESIZE) {
        constexpr uint64_t page_mask = (1ULL<<30) - 1;
        return physical_address{(pdpe & ~(PAGEF_NX | page_mask)) | (virt & page_mask)};
    }

    const auto pde = table_entry(pdpe)[virt.pde()];
    //dbgout() << "PD   " << as_hex(pde) << "\n";
    REQUIRE(pde & PAGEF_PRESENT);
    if (pde & PAGEF_PAGESIZE) {
        constexpr uint64_t page_mask = (2ULL<<20) - 1;
        return physical_address{(pde & ~(PAGEF_NX | page_mask)) | (virt & page_mask)};
    }

    const auto pte = table_entry(pde)[virt.pte()];
    //dbgout() << "PT   " << as_hex(pte) << "\n";
    REQUIRE(pte & PAGEF_PRESENT);

    return physical_address{(pte & ~(PAGEF_NX | (memory_manager::page_size-1))) | (virt & (memory_manager::page_size-1)) };
}

physical_address virt_to_phys(const void* ptr)
{
    return virt_to_phys(physical_address{__readcr3()}, virtual_address::in_current_address_space(ptr));
}

memory_manager& kmemory_manager()
{
    return kernel_memory_manager::instance();
}

volatile void* iomem_map(physical_address base, uint64_t length)
{
    REQUIRE((base & (memory_manager::page_size-1)) == 0);
    const auto virt = kernel_memory_manager::instance().map_memory(memory_manager::map_alloc_virt, length, memory_type_rw | memory_type::cache_disable, base);
    return reinterpret_cast<volatile void*>(static_cast<uint64_t>(virt));
}

void iomem_unmap(volatile void* virt, uint64_t length)
{
    kernel_memory_manager::instance().unmap_memory(virtual_address::in_current_address_space(const_cast<void*>(virt)), length);
}

physical_allocation alloc_physical(uint64_t bytes) {
    return kernel_memory_manager::instance().alloc_physical(bytes);
}

void free_physical_page(physical_address addr) { // Internal use only
    REQUIRE(!(addr & (memory_manager::page_size-1)));
    return kernel_memory_manager::instance().free_physical(addr, memory_manager::page_size);
}

class default_memory_manager : public memory_manager_base {
public:
    explicit default_memory_manager() {
        // The mm is born with the current high mem (kernel) mapping
        static_cast<uint64_t*>(pml4())[kernel_pml4] = static_cast<const uint64_t*>(kernel_memory_manager::instance().pml4())[kernel_pml4];
    }

    ~default_memory_manager() {
        REQUIRE(static_cast<uint64_t*>(pml4())[kernel_pml4] == static_cast<const uint64_t*>(kernel_memory_manager::instance().pml4())[kernel_pml4]);
        static_cast<uint64_t*>(pml4())[kernel_pml4] = 0;
    }

private:
    virtual virtual_address do_map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) override {
        REQUIRE(virt.pml4e() != kernel_pml4); // Make sure we don't clobber the kernel pml4
        REQUIRE(virt.pml4e() < 0x100); // For now don't allow mappings in "high mem" (0x0xffff0000`00000000-0xffffffff`ffffffff)
        return memory_manager_base::do_map_memory(virt, length, type, phys);
    }
};

kowned_ptr<memory_manager> create_default_memory_manager() {
    return kowned_ptr<memory_manager>{knew<default_memory_manager>().release()};
}

void* kalloc(uint64_t size) {
    return kernel_memory_manager::instance().alloc(size);
}

void kfree(void* ptr) {
    kernel_memory_manager::instance().free(ptr);
}

} // namespace attos

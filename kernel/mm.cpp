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

physical_address alloc_physical_page() { return alloc_physical(memory_manager::page_size); }

constexpr virtual_address kernel_map_start{0xFFFFFFFF'FF000000};

class memory_manager_base : public memory_manager {
public:
    explicit memory_manager_base()
        : memory_mappings_{alloc_physical_page(), page_size}
        , pml4_{static_cast<uint64_t*>(alloc_physical_page())}
        , free_virt_base_{kernel_map_start-(1ULL<<30)} { // HACK: ISR needs virtual memory within 2GB of the kernel code...
    }

    ~memory_manager_base() {
        // TODO: Free stuff...
    }

    memory_manager_base(const memory_manager_base&) = delete;
    memory_manager_base& operator=(const memory_manager_base&) = delete;

private:
    fixed_size_object_heap<memory_mapping> memory_mappings_;
    memory_mapping::tree_type              memory_map_tree_;
    uint64_t*                              pml4_;
    virtual_address                        free_virt_base_;

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
        auto table = static_cast<uint64_t*>(alloc_physical_page());
        parent = physical_address::from_identity_mapped_ptr(table) | flags;
        //dbgout() << "[mem] Allocated page table. parent " << as_hex((uint64_t)&parent) << " <- " << as_hex(parent) << "\n";
        return table;
    }

    virtual physical_address do_pml4() const override {
        return physical_address::from_identity_mapped_ptr(pml4_);
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

    virtual virtual_address do_map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) override {
        dbgout() << "[mem] map " << as_hex(virt) << " <- " << as_hex(phys) << " len " << as_hex(length).width(0) << ' ' << type << "\n";

        const uint64_t map_page_size = static_cast<uint32_t>(type & memory_type::ps_1gb) ? (1<<30) : static_cast<uint32_t>(type & memory_type::ps_2mb) ? (2<<20) : (1<<12);

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
};

// Simple heap. The free blocks are kept in list sorted according to memory address. Memory blocks are coalesced on free().
class simple_heap {
public:
    static constexpr uint64_t alignment = 16;

    explicit simple_heap(uint8_t* base, uint64_t length) : base_(base), end_(base + length), free_(end_of_list) {
        REQUIRE(!((uint64_t)base & (alignment-1)));
        REQUIRE(length >= alignment);
        insert_free(base, length);
    }

    ~simple_heap() {
        REQUIRE(free_ == reinterpret_cast<free_node*>(base_));
        REQUIRE(free_->size == static_cast<uint64_t>(end_ - base_));
    }

    simple_heap(const simple_heap&) = delete;
    simple_heap& operator=(const simple_heap&) = delete;

    uint8_t* alloc(uint64_t size) {
        //dbgout() << "simple_heap::alloc(" << as_hex(size) << ")\n";
        size = round_up(size + alignment, alignment);

        free_node** fp = &free_;
        while ((*fp) != end_of_list && (*fp)->size < size) {
            fp = &(*fp)->next;
        }
        REQUIRE(*fp != end_of_list && "Could not satisfy allocation request");

        uint8_t* res = reinterpret_cast<uint8_t*>(*fp);
        free_node* next_free     = *fp + size / sizeof(free_node);
        const uint64_t free_size = (*fp)->size - size;
        if (free_size) {
            next_free->size = free_size;
            next_free->next = (*fp)->next;
            //dbgout() << "simple_heap took from block " << **fp << " - new block " << *next_free << "\n";
            *fp = next_free;
        } else {
            //dbgout() << "simple_heap took complete block " << **fp << "\n";
            *fp = (*fp)->next;
        }
        *reinterpret_cast<uint64_t*>(res) = size;
        //dbgout() << "simple_heap::alloc() returning " << as_hex((uint64_t)(res + alignment)) << "\n";
        return res + alignment;
    }

    void free(uint8_t* ptr) {
        REQUIRE((uint64_t)ptr >= (uint64_t)base_ && (uint64_t)ptr + alignment <= (uint64_t)end_);
        ptr -= alignment;
        const uint64_t size = *reinterpret_cast<uint64_t*>(ptr);
        //dbgout() << "simple_heap::free(" << as_hex((uint64_t)ptr) << ") size = "  << as_hex(size) << "\n";
        insert_free(ptr, size);
    }

private:
    uint8_t* const base_;
    uint8_t* const end_;

    void insert_free(void* ptr, uint64_t size) {
        auto new_free = reinterpret_cast<free_node*>(ptr);
        new_free->size = size;
        new_free->next = end_of_list;

        // If the free list is empty or we're the new head
        if (free_ == end_of_list || free_ > new_free) {
            //dbgout() << "Inserting " << *new_free << " as new list head\n";
            new_free->next = free_;
            free_ = new_free;
            coalesce_from(free_);
            return;
        }

        // Find the node just before our insertion point
        free_node* prev = free_;
        for (; prev->next != end_of_list && prev->next < new_free; prev = prev->next)
            ;

        //dbgout() << "Inserting " << *new_free << " after " << *prev << " before ";
        //if (prev->next != end_of_list)
        //    dbgout() << *prev->next << "\n";
        //else
        //    dbgout() << "end_of_list\n";
        new_free->next = prev->next;
        prev->next = new_free;

        // Coalescing blocks starting from our insertion point
        coalesce_from(prev);
    }

    struct free_node {
        uint64_t   size;
        free_node* next;

        friend out_stream& operator<<(out_stream& os, const free_node& n) {
            return os << as_hex((uint64_t)&n) << "{" << as_hex(n.size).width(6) << ", " << as_hex((uint64_t)n.next) << "}";
        }
    };
    static_assert(sizeof(free_node) == alignment, "");
    static constexpr free_node* end_of_list = reinterpret_cast<free_node*>(~0ULL);
    free_node* free_;

    void print_free(out_stream& os) {
        os << "simple_heap free:\n";
        for (free_node* f = free_; f != end_of_list; f = f->next) {
            os << " " << *f << "\n";
        }
    }

    static void coalesce_from(free_node* f) {
        while (f != end_of_list && f->next != end_of_list && reinterpret_cast<uint64_t>(f) + f->size == reinterpret_cast<uint64_t>(f->next)) {
            //dbgout() << "coalesce " << *f << " with " << *f->next << "\n";
            f->size += f->next->size;
            f->next  = f->next->next;
        }
    }

};

constexpr uint64_t initial_heap_size = 1<<20;

class kernel_memory_manager : public memory_manager, public singleton<kernel_memory_manager> {
public:
    explicit kernel_memory_manager(physical_address base, uint64_t length)
        : saved_cr3_(__readcr3())
        , physical_pages_{base, length}
        , kernel_heap_{static_cast<uint8_t*>(alloc_physical(initial_heap_size)), initial_heap_size} {
        dbgout() << "[mem] Starting. Base 0x" << as_hex(base) << " Length " << (length>>20) << " MB\n";
        mm_ = mm_buffer_.construct();
    }

    ~kernel_memory_manager() {
        dbgout() << "[mem] Shutting down. Restoring CR3 to " << as_hex(saved_cr3_) << "\n";
        __writecr3(saved_cr3_);
    }

    kernel_memory_manager(const kernel_memory_manager&) = delete;
    kernel_memory_manager& operator=(const kernel_memory_manager&) = delete;

    physical_address alloc_physical(uint64_t size) {
        size = round_up(size, page_size);
        auto ptr = physical_pages_.alloc(size);
        __stosq(reinterpret_cast<uint64_t*>(ptr), 0, size / 8);
        return physical_address::from_identity_mapped_ptr(ptr);
    }

    void* alloc(uint64_t size) {
        return kernel_heap_.alloc(size);
    }

    void free(void* ptr) {
        kernel_heap_.free(reinterpret_cast<uint8_t*>(ptr));
    }
private:
    physical_address                                 saved_cr3_;
    no_free_heap<page_size>                          physical_pages_;
    simple_heap                                      kernel_heap_;
    object_buffer<memory_manager_base>               mm_buffer_;
    owned_ptr<memory_manager_base, destruct_deleter> mm_;

    virtual physical_address do_pml4() const override {
        return mm_->pml4();
    }

    virtual virtual_address do_map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) override {
        return mm_->map_memory(virt, length, type, phys);
    }
};
object_buffer<kernel_memory_manager> mm_buffer;

memory_manager_ptr mm_init(physical_address base, uint64_t length)
{
    auto mm = mm_buffer.construct(base, length);
    return memory_manager_ptr{mm.release()};
}

void print_page_tables(physical_address pml4_address)
{
    dbgout() << "pml4 = " << as_hex(pml4_address) << "\n";
    auto pml4 = table_entry(pml4_address);
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
    dbgout() << "[mem] Ignoring I/O mem unmap request virt = " << as_hex((uint64_t)virt) << " length = " << as_hex(length) << "\n";
}

physical_address alloc_physical(uint64_t bytes) {
    return kernel_memory_manager::instance().alloc_physical(bytes);
}

kowned_ptr<memory_manager> create_default_memory_manager() {
    auto mmb = knew<memory_manager_base>();
    // The mm is born with the current high mem (kernel) mapping
    move_memory(static_cast<uint64_t*>(mmb->pml4()) + 256, static_cast<const uint64_t*>(kernel_memory_manager::instance().pml4()) + 256, 256 * sizeof(uint64_t));
    return kowned_ptr<memory_manager>{mmb.release()};
}

void* kalloc(uint64_t size) {
    return kernel_memory_manager::instance().alloc(size);
}

void kfree(void* ptr) {
    kernel_memory_manager::instance().free(ptr);
}

} // namespace attos

#include "mm.h"
#include "cpu.h"
#include "out_stream.h"

#define assert(expr)
#include "tree.h"
#undef assert

namespace attos {

inline uint64_t* table_entry(uint64_t table_value) {
    return static_cast<uint64_t*>(physical_address{table_value & ~(memory_manager::page_size - 1)});
}


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

class kernel_memory_manager : public memory_manager {
public:
    static kernel_memory_manager* instance() {
        return instance_;
    }

    explicit kernel_memory_manager(physical_address base, uint64_t length)
        : physical_pages_{base, length}
        , memory_mappings_{alloc_physical(page_size), page_size}
        , saved_cr3_(__readcr3()) {
        dbgout() << "[mem] Starting. Base 0x" << as_hex(base) << " Length " << (length>>20) << " MB\n";
        pml4_ = static_cast<uint64_t*>(alloc_physical(page_size));
        instance_ = this;
    }

    ~kernel_memory_manager() {
        dbgout() << "[mem] Shutting down. Restoring CR3 to " << as_hex(saved_cr3_) << "\n";
        instance_ = nullptr;
        __writecr3(saved_cr3_);
    }

    kernel_memory_manager(const kernel_memory_manager&) = delete;
    kernel_memory_manager& operator=(const kernel_memory_manager&) = delete;

    physical_address saved_cr3() const {
        return saved_cr3_;
    }

    physical_address pml4() const {
        return physical_address::from_identity_mapped_ptr(pml4_);
    }

    physical_address alloc_physical(uint64_t size) {
        size = round_up(size, page_size);
        auto ptr = physical_pages_.alloc(size);
        __stosq(reinterpret_cast<uint64_t*>(ptr), 0, size / 8);
        return physical_address::from_identity_mapped_ptr(ptr);
    }

private:
    simple_heap<page_size>                 physical_pages_;
    fixed_size_object_heap<memory_mapping> memory_mappings_;
    memory_mapping::tree_type              memory_map_tree_;
    physical_address                       saved_cr3_;
    uint64_t*                              pml4_;

    static kernel_memory_manager*          instance_;

    uint64_t* alloc_if_not_present(uint64_t& parent, uint64_t flags) {
        if (parent & PAGEF_PRESENT) {
            // TODO: Check flags
            return table_entry(parent);
        }
        return alloc_table_entry(parent, flags);
    }

    uint64_t* alloc_table_entry(uint64_t& parent, uint64_t flags) {
        auto table = static_cast<uint64_t*>(alloc_physical(page_size));
        parent = physical_address::from_identity_mapped_ptr(table) | PAGEF_PRESENT | flags;
        dbgout() << "[mem] Allocated page table. parent " << as_hex((uint64_t)&parent) << " <- " << as_hex(parent) << "\n";
        return table;
    }

    virtual void do_ready() override {
        REQUIRE(__readcr3() == saved_cr3());
        __writecr3(pml4());
    }

    virtual void do_map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) override {
        dbgout() << "[mem] map " << as_hex(virt) << " <- " << as_hex(phys) << " len " << as_hex(length).width(0) << " type = " << as_hex(type).width(0) << "\n";

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
            dbgout() << "[mem] FATAL ERROR overlaps " << as_hex(it->address()) << "\n";
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
            dbgout() << "[mem] " << as_hex(virt) << " " << as_hex(phys) << "\n";
        }
    }
};
kernel_memory_manager* kernel_memory_manager::instance_ = nullptr;
object_buffer<kernel_memory_manager> mm_buffer;

owned_ptr<memory_manager, destruct_deleter> mm_init(physical_address base, uint64_t length)
{
    REQUIRE(kernel_memory_manager::instance() == nullptr);
    auto mm = mm_buffer.construct(base, length);
    return owned_ptr<memory_manager, destruct_deleter>{mm.release()};
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

physical_address virt_to_phys(physical_address cr3, virtual_address virt)
{
    //dbgout() << "virt_to_phys(" << as_hex(cr3) << ", " << as_hex(virt) << ")\n";

    const auto pml4e = table_entry(cr3)[virt.pml4e()];
    //dbgout() << "PML4 " << as_hex(pml4e) << "\n";
    REQUIRE(pml4e & PAGEF_PRESENT);

    const auto pdpe  = table_entry(pml4e)[virt.pdpe()];
    //dbgout() << "PDP  " << as_hex(pdpe) << "\n";
    REQUIRE(pdpe & PAGEF_PRESENT);
    if (pdpe & PAGEF_PAGESIZE) {
        constexpr uint64_t page_mask = (1ULL<<30) - 1;
        return physical_address{(pdpe & ~page_mask) | (virt & page_mask)};
    }

    const auto pde = table_entry(pdpe)[virt.pde()];
    //dbgout() << "PD   " << as_hex(pde) << "\n";
    REQUIRE(pde & PAGEF_PRESENT);
    if (pde & PAGEF_PAGESIZE) {
        constexpr uint64_t page_mask = (2ULL<<20) - 1;
        return physical_address{(pdpe & ~page_mask) | (virt & page_mask)};
    }

    const auto pte = table_entry(pde)[virt.pte()];
    //dbgout() << "PT   " << as_hex(pte) << "\n";
    REQUIRE(pte & PAGEF_PRESENT);

    return physical_address{(pte & ~(memory_manager::page_size-1)) | (virt & (memory_manager::page_size-1)) };
}

} // namespace attos

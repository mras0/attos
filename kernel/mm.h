#ifndef ATTOS_MM_H
#define ATTOS_MM_H

#include <attos/mem.h>
#include <attos/containers.h>

namespace attos {

class __declspec(novtable) memory_manager {
public:
    virtual ~memory_manager() {}

    static constexpr uint64_t page_size = 4096;

    static constexpr virtual_address map_alloc_virt{static_cast<uint64_t>(-1)};
    static constexpr virtual_address map_alloc_virt_close_to_kernel{static_cast<uint64_t>(-2)};

    void switch_to() {
        do_switch_to();
    }

    virtual_address map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) {
        return do_map_memory(virt, length, type, phys);
    }

    void unmap_memory(virtual_address virt, uint64_t length) {
        do_unmap_memory(virt, length);
    }

private:
    virtual virtual_address do_map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) = 0;
    virtual void do_unmap_memory(virtual_address virt, uint64_t length) = 0;
    virtual void do_switch_to() = 0;
};

using memory_manager_ptr = owned_ptr<memory_manager, destruct_deleter>;

memory_manager_ptr mm_init(physical_address base, uint64_t length);
void print_page_tables(physical_address pml4);

physical_address virt_to_phys(physical_address pml4, virtual_address virt);
physical_address virt_to_phys(const void* ptr); // Pointer must be in current address space

volatile void* iomem_map(physical_address base, uint64_t length);
void iomem_unmap(volatile void* virt, uint64_t length);

memory_manager& kmemory_manager();

class kernel_memory_manager;
class memory_manager_base;

class physical_allocation {
public:
    physical_allocation(const physical_allocation&) = delete;
    physical_allocation& operator=(const physical_allocation&) = delete;
    physical_allocation(physical_allocation&& other) : addr_(other.addr_), length_(other.length_) {
        other.length_ = 0;
    }
    physical_allocation& operator=(physical_allocation&& other);
    ~physical_allocation();

    constexpr physical_address address() const { return addr_; }
    constexpr uint64_t length() const { return length_; }

private:
    constexpr physical_allocation(physical_address addr, uint64_t length) : addr_(addr), length_(length) {
    }

    physical_address release();

    friend kernel_memory_manager;
    friend memory_manager_base;
    physical_address addr_;
    uint64_t         length_;
};

physical_allocation alloc_physical(uint64_t bytes);

kowned_ptr<memory_manager> create_default_memory_manager();

} // namespace attos
#endif

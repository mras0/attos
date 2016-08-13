#ifndef ATTOS_MM_H
#define ATTOS_MM_H

#include <attos/mem.h>
#include <attos/containers.h>

namespace attos {

class __declspec(novtable) memory_manager {
public:
    virtual ~memory_manager() {}

    static constexpr uint64_t page_size = 4096;

    physical_address pml4() const {
        return do_pml4();
    }

    static constexpr virtual_address map_alloc_virt{static_cast<uint64_t>(-1)};
    static constexpr virtual_address map_alloc_virt_close_to_kernel{static_cast<uint64_t>(-2)};

    virtual_address map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) {
        return do_map_memory(virt, length, type, phys);
    }

private:
    virtual physical_address do_pml4() const = 0;
    virtual virtual_address do_map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) = 0;
};

using memory_manager_ptr = owned_ptr<memory_manager, destruct_deleter>;

memory_manager_ptr mm_init(physical_address base, uint64_t length);
void print_page_tables(physical_address pml4);

physical_address virt_to_phys(physical_address pml4, virtual_address virt);
physical_address virt_to_phys(const void* ptr); // Pointer must be in current address space

volatile void* iomem_map(physical_address base, uint64_t length);
void iomem_unmap(volatile void* virt, uint64_t length);

memory_manager& kmemory_manager();

physical_address alloc_physical(uint64_t bytes);

kowned_ptr<memory_manager> create_default_memory_manager();

} // namespace attos
#endif

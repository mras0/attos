#ifndef ATTOS_MM_H
#define ATTOS_MM_H

#include <attos/mem.h>

namespace attos {

class __declspec(novtable) memory_manager {
public:
    virtual ~memory_manager() {}

    static constexpr uint64_t page_size = 4096;

    void map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) {
        do_map_memory(virt, length, type, phys);
    }

private:
    virtual void do_map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) = 0;
};

owned_ptr<memory_manager, destruct_deleter> mm_init(physical_address base, uint64_t length);
void mm_ready();

} // namespace attos
#endif

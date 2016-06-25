#ifndef ATTOS_MM_H
#define ATTOS_MM_H

#include <attos/mem.h>

namespace attos {

class __declspec(novtable) memory_manager {
public:
    virtual ~memory_manager() {}

    static constexpr uint64_t page_size = 4096;

    void ready() {
        do_ready();
    }

    void map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) {
        do_map_memory(virt, length, type, phys);
    }

private:
    virtual void do_ready() = 0;
    virtual void do_map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) = 0;
};

owned_ptr<memory_manager, destruct_deleter> mm_init(physical_address base, uint64_t length);
void print_page_tables(physical_address cr3);

physical_address virt_to_phys(physical_address cr3, virtual_address virt);

} // namespace attos
#endif

#ifndef ATTOS_CPU_MANAGER_H
#define ATTOS_CPU_MANAGER_H

#include <attos/cpu.h>

namespace attos {

static constexpr uint16_t kernel_cs = 0x08;
static constexpr uint16_t kernel_ds = 0x10;
static constexpr uint16_t user_cs   = 0x23;
static constexpr uint16_t user_ds   = 0x1b;

class __declspec(novtable) cpu_manager {
public:
    virtual ~cpu_manager() {}

    void switch_to_context(uint64_t cs, uint64_t rip, uint64_t ss, uint64_t rsp, uint64_t flags) {
        do_switch_to_context(cs, rip, ss, rsp, flags);
    }

private:
    virtual void do_switch_to_context(uint64_t cs, uint64_t rip, uint64_t ss, uint64_t rsp, uint64_t flags) = 0;
};

owned_ptr<cpu_manager, destruct_deleter> cpu_init();

void restore_original_context();

}  // namespace attos

#endif

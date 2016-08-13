#ifndef ATTOS_CPU_MANAGER_H
#define ATTOS_CPU_MANAGER_H

#include <attos/cpu.h>
#include <attos/function.h>

namespace attos {

static constexpr uint16_t kernel_cs = 0x08;
static constexpr uint16_t kernel_ds = 0x10;
static constexpr uint16_t user_cs   = 0x23;
static constexpr uint16_t user_ds   = 0x1b;

class __declspec(novtable) cpu_manager {
public:
    virtual ~cpu_manager() {}

    void switch_to_context(registers& regs) {
        do_switch_to_context(regs);
    }

private:
    virtual void do_switch_to_context(registers& regs) = 0;
};

owned_ptr<cpu_manager, destruct_deleter> cpu_init();

void restore_original_context();

using syscall_handler_t = function<void (registers&)>;
class syscall_enabler {
public:
    explicit syscall_enabler(syscall_handler_t handler);
    ~syscall_enabler();
};

}  // namespace attos

#endif

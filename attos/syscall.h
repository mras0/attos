#ifndef ATTOS_SYSCALL_H
#define ATTOS_SYSCALL_H

namespace attos {

enum class syscall_number : uint64_t {
    exit        = 0,
    debug_print = 1,

    // HACK/TEST
    ethdev_hw_address = 0x1000,
};

extern "C" void syscall1(syscall_number n, uint64_t arg0);
extern "C" void syscall2(syscall_number n, uint64_t arg0, uint64_t arg1);

} // namespace attos
#endif

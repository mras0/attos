#ifndef ATTOS_SYSCALL_H
#define ATTOS_SYSCALL_H

namespace attos {

enum class syscall_number : uint64_t {
    exit = 0,
    debug_print,
    yield,

    // HACK/TEST
    ethdev_create = 0x1000,
    ethdev_destroy,
    ethdev_hw_address,
    ethdev_send,
    ethdev_recv,
};

extern "C" void syscall0(syscall_number n);
extern "C" void syscall1(syscall_number n, uint64_t arg0);
extern "C" void syscall2(syscall_number n, uint64_t arg0, uint64_t arg1);
extern "C" void syscall3(syscall_number n, uint64_t arg0, uint64_t arg1, uint64_t arg2);

} // namespace attos
#endif

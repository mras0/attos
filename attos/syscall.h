#ifndef ATTOS_SYSCALL_H
#define ATTOS_SYSCALL_H

namespace attos {

enum class syscall_number : uint64_t {
    exit = 0,
    debug_print,
    yield,

    create,
    destroy,
    read,
    write,

    // HACK/TEST
    ethdev_hw_address,

    start_exe,
    process_exit_code,

    mem_map_info,
};

extern "C" uint64_t syscall0(syscall_number n);
extern "C" uint64_t syscall1(syscall_number n, uint64_t arg0);
extern "C" uint64_t syscall2(syscall_number n, uint64_t arg0, uint64_t arg1);
extern "C" uint64_t syscall3(syscall_number n, uint64_t arg0, uint64_t arg1, uint64_t arg2);

} // namespace attos
#endif

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

} // namespace attos
#endif

#ifndef ATTOS_SYSUSER_H
#define ATTOS_SYSUSER_H

#include <attos/syscall.h>

namespace attos {

// syscall.asm
extern "C" uint64_t syscall0(syscall_number n);
extern "C" uint64_t syscall1(syscall_number n, uint64_t arg0);
extern "C" uint64_t syscall2(syscall_number n, uint64_t arg0, uint64_t arg1);
extern "C" uint64_t syscall3(syscall_number n, uint64_t arg0, uint64_t arg1, uint64_t arg2);

class sys_handle {
public:
    explicit sys_handle(const char* name) : id_(syscall1(syscall_number::create, (uint64_t)name)) {
    }
    ~sys_handle() {
        syscall1(syscall_number::destroy, id_);
    }
    sys_handle(const sys_handle&) = delete;
    sys_handle& operator=(const sys_handle&) = delete;

    uint64_t id() const { return id_; }

private:
    uint64_t id_;
};

inline void write(sys_handle& h, const void* data, uint32_t length) {
    syscall3(syscall_number::write, h.id(), (uint64_t)data, length);
}

inline uint32_t read(sys_handle& h, void* data, uint32_t max) {
    return static_cast<uint32_t>(syscall3(syscall_number::read, h.id(), (uint64_t)data, max));
}


} // namespace attos
#endif

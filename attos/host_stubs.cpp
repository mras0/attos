#include <attos/out_stream.h>
#include <attos/string.h>
#include <attos/net/net.h>
#include <attos/net/tftp.h>
#include <attos/cpu.h>

namespace attos {
void* kalloc(uint64_t size) {
    dbgout() << "Ignoring alloc of " << as_hex(size) << "\n";
    abort();
}

void kfree(void* ptr) {
    dbgout() << "Ignoring free of " << as_hex((uint64_t)ptr) << "\n";
    abort();
}

void yield() {
    dbgout() << "yield() called\n";
    abort();
}

void fatal_error(const char* file, int line, const char* detail) {
    dbgout() << file << ':' << line << ": " << detail << ".\nQuitting\n";
    abort();
}


} // namespace attos

#include <iostream>

class attos_stream_wrapper : public attos::out_stream {
public:
    explicit attos_stream_wrapper(std::ostream& os) : os_(os) {
        attos::set_dbgout(*this);
    }
    virtual void write(const void* data, size_t n) {
        os_.write(reinterpret_cast<const char*>(data), n);
    }
private:
    std::ostream& os_;
};
attos_stream_wrapper asw{std::cout};

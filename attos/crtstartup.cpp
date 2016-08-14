#include <attos/out_stream.h>
#include <attos/syscall.h>
#include <attos/cpu.h>

using namespace attos;

extern "C" int main();

namespace attos {

alignas(16) uint8_t stupid_heap[4096*8];
uint64_t heap_index = 0;

void* kalloc(uint64_t size) {
    auto p = &stupid_heap[heap_index];
    heap_index += round_up(size, 16);
    REQUIRE(heap_index <= sizeof(stupid_heap));
    return p;
}

void kfree(void* ptr) {
    dbgout() << "Ignoring free of " << as_hex((uint64_t)ptr) << "\n";
}

void yield()
{
    syscall0(syscall_number::yield);
}

void exit(uint64_t ret)
{
    syscall1(syscall_number::exit, ret);
}

void fatal_error(const char* file, int line, const char* detail)
{
    dbgout() << file << ':' << line << ": " << detail << ".\nQuitting\n";
    bochs_magic();
    exit(static_cast<uint64_t>(-42));
}

} // namespace attos

class debug_out_printer : public attos::out_stream {
public:
    explicit debug_out_printer() {
        attos::set_dbgout(*this);
    }
    virtual void write(const void* data, size_t n) override {
        syscall2(syscall_number::debug_print, reinterpret_cast<uint64_t>(data), n);
    }
};

extern "C" void mainCRTStartup()
{
    debug_out_printer dop{};
    attos::exit(main());
}

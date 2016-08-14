#include <attos/out_stream.h>
#include <attos/syscall.h>
#include <attos/cpu.h>

using namespace attos;

extern "C" int main();

namespace attos {

namespace {
alignas(16) uint8_t stupid_heap[4096*8];
default_heap* heap_ptr;
} // unnamed namespace

void* kalloc(uint64_t size) {
    return heap_ptr->alloc(size);
}

void kfree(void* ptr) {
    heap_ptr->free(ptr);
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
    default_heap heap{stupid_heap, sizeof(stupid_heap)};
    heap_ptr = &heap;
    attos::exit(main());
    heap_ptr = nullptr;
}

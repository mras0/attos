#include <attos/out_stream.h>
#include <attos/syscall.h>

using namespace attos;

extern "C" int main();

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
    syscall1(syscall_number::exit, main());
}

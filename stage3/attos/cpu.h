#ifndef ATTOS_CPU_H
#define ATTOS_CPU_H

#include <stdint.h>
#include <intrin.h>

#pragma intrinsic(_disable)
#pragma intrinsic(_enable)
extern "C" void* memset(void* dest, int c, size_t count); // crt.asm
#pragma function(memset)

#include <attos/mem.h>

#define REQUIRE(expr) do { if (!(expr)) { ::attos::fatal_error(__FILE__, __LINE__, #expr " failed"); } } while (0)

namespace attos {

extern void (*bochs_magic)();

template<uint8_t InterruptNo>
void sw_int() {
    static constexpr uint8_t code[] = { 0xCD, InterruptNo, 0xC3 }; // int InterruptNo; ret
    ((void (*)(void))(void*)code)();
}

__declspec(noreturn) void fatal_error(const char* file, int line, const char* detail);

template<typename T>
constexpr auto round_up(T val, T align)
{
    return val % align ? val + align - (val % align) : val;
}

class __declspec(novtable) cpu_manager {
public:
    virtual ~cpu_manager() {}
    static constexpr uint16_t kernel_cs = 0x08;
    static constexpr uint16_t kernel_ds = 0x10;
};

owned_ptr<cpu_manager, destruct_deleter> cpu_init();

}  // namespace attos

#endif

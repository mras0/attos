#ifndef ATTOS_CPU_H
#define ATTOS_CPU_H

#include <stdint.h>
#include <intrin.h>

#pragma intrinsic(_disable)
#pragma intrinsic(_enable)

#define REQUIRE(expr) do { if (!(expr)) { ::attos::fatal_error(__FILE__, __LINE__, #expr " failed"); } } while (0)

namespace attos {

constexpr uint16_t kernel_cs = 0x28; // Matches stage2/bootloader.asm

namespace detail {
static constexpr uint8_t bochs_magic_code[] = { 0x66, 0x87, 0xDB, 0xC3 }; // xchg bx, bx; ret
} // namespace detail
static auto bochs_magic = ((void (*)(void))(void*)detail::bochs_magic_code);

void fatal_error(const char* file, int line, const char* detail);

template<typename T>
constexpr auto round_up(T val, T align)
{
    return val % align ? val + align - (val % align) : val;
}

}  // namespace attos

#endif

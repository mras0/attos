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

enum rflag_bits {
    rflag_bit_cf   = 0,  // carry
    rflag_bit_res1 = 1,  // reserved (always 1)
    rflag_bit_pf   = 2,  // parity
                         // reserved
    rflag_bit_af   = 4,  // adjust
                         // reserved
    rflag_bit_zx   = 6,  // zero
    rflag_bit_sf   = 7,  // sign
    rflag_bit_tf   = 8,  // trap
    rflag_bit_if   = 9,  // interrupt
    rflag_bit_df   = 10, // direction
    rflag_bit_of   = 11, // overflow
    rflag_bit_iopl = 12, // iopl
                         // iopl
    rflag_bit_nt   = 14, // nested task
};

constexpr auto rflag_mask_if = 1ULL << rflag_bit_if;

class interrupt_disabler {
public:
    interrupt_disabler() : were_interrupts_enabled_((__readeflags() & rflag_mask_if) != 0) {
        _disable();
    }
    ~interrupt_disabler() {
        if (were_interrupts_enabled_) {
            _enable();
        }
    }
    interrupt_disabler(const interrupt_disabler&) = delete;
    interrupt_disabler& operator=(const interrupt_disabler&) = delete;
private:
    bool were_interrupts_enabled_;
};

}  // namespace attos

#endif

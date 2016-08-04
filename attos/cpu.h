#ifndef ATTOS_CPU_H
#define ATTOS_CPU_H

#include <stdint.h>
#include <intrin.h>

#pragma intrinsic(_disable)
#pragma intrinsic(_enable)
extern "C" void* memset(void* dest, int c, size_t count); // crt.asm
#pragma function(memset)
extern "C" void* memcpy(void* dest, const void* src, size_t count); // crt.asm
#pragma function(memset)

#include <attos/mem.h>

#define FATAL_ERROR(msg) ::attos::fatal_error(__FILE__, __LINE__, (msg))
#define REQUIRE(expr) do { if (!(expr)) { FATAL_ERROR(#expr " failed"); } } while (0)

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

static constexpr uint16_t kernel_cs = 0x08;
static constexpr uint16_t kernel_ds = 0x10;
static constexpr uint16_t user_cs   = 0x23;
static constexpr uint16_t user_ds   = 0x1b;

class __declspec(novtable) cpu_manager {
public:
    virtual ~cpu_manager() {}

    void switch_to_context(uint64_t cs, uint64_t rip, uint64_t ss, uint64_t rsp, uint64_t flags) {
        do_switch_to_context(cs, rip, ss, rsp, flags);
    }

private:
    virtual void do_switch_to_context(uint64_t cs, uint64_t rip, uint64_t ss, uint64_t rsp, uint64_t flags) = 0;
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

enum class interrupt_number : uint8_t;

// Must match the structure in isr_common.asm
struct registers {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint8_t  fx_state[512];
    uint64_t reserved0;
    interrupt_number interrupt_no;
    uint8_t  reserved1[7];
    uint64_t error_code;
    uint64_t rip;
    uint16_t cs;
    uint8_t  reserved2[6];
    uint32_t eflags;
    uint8_t  reserved3[4];
    uint64_t rsp;
    uint16_t ss;
    uint8_t  reserved4[6];
};

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

enum class descriptor_privilege_level : uint8_t {
    kernel = 0,
    user   = 3
};

void restore_switch_to_context(registers& r);

}  // namespace attos

#endif

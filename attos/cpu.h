#ifndef ATTOS_CPU_H
#define ATTOS_CPU_H

#include <stdint.h>
#include <intrin.h>

#pragma intrinsic(_disable)
#pragma intrinsic(_enable)
extern "C" void* memset(void* dest, int c, size_t count); // crt.asm
#pragma function(memset)
extern "C" void* memcpy(void* dest, const void* src, size_t count); // crt.asm
#pragma function(memcpy)

#include <attos/mem.h>

#define FATAL_ERROR(msg) ::attos::fatal_error(__FILE__, __LINE__, (msg))
#define REQUIRE(expr) do { if (!(expr)) { FATAL_ERROR(#expr " failed"); } } while (0)

namespace attos {

extern "C" void bochs_magic();

__declspec(noreturn) void fatal_error(const char* file, int line, const char* detail);
void yield();

template<typename T, typename U>
constexpr auto round_up(T val, U align)
{
    return val % align ? val + align - (val % align) : val;
}

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

    rflag_bit_ac   = 18, // alignment check
};

constexpr auto rflag_mask_res1 = 1U << rflag_bit_res1;
constexpr auto rflag_mask_tf   = 1U << rflag_bit_tf;
constexpr auto rflag_mask_if   = 1U << rflag_bit_if;
constexpr auto rflag_mask_df   = 1U << rflag_bit_df;
constexpr auto rflag_mask_iopl = 3U << rflag_bit_iopl;
constexpr auto rflag_mask_ac   = 1U << rflag_bit_ac;

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

constexpr uint8_t pf_error_code_mask_p = 0x01; // Page fault was caused by a page-protection violation
constexpr uint8_t pf_error_code_mask_w = 0x02; // Page fault was caused by a page write
constexpr uint8_t pf_error_code_mask_u = 0x04; // Page fault was caused while CPL = 3
constexpr uint8_t pf_error_code_mask_r = 0x08; // Page fault was caused by reading a 1 in a reserved field
constexpr uint8_t pf_error_code_mask_i = 0x10; // Page fault was caused by an instruction fetch

//
// Model Specific Registers (MSRs)
//

constexpr uint32_t msr_efer           = 0xc0000080; // extended feature register
constexpr uint32_t msr_star           = 0xc0000081; // legacy mode SYSCALL target
constexpr uint32_t msr_lstar          = 0xc0000082; // long mode SYSCALL target
constexpr uint32_t msr_cstar          = 0xc0000083; // compat mode SYSCALL target
constexpr uint32_t msr_fmask          = 0xc0000084; // EFLAGS mask for syscall
constexpr uint32_t msr_fs_base        = 0xc0000100; // FS base
constexpr uint32_t msr_gs_base        = 0xc0000101; // GS base
constexpr uint32_t msr_kernel_gs_base = 0xc0000102; // SWAPGS shadow base

constexpr uint32_t efer_mask_sce   = 1U<<0;  // SysCall exetension
constexpr uint32_t efer_mask_lme   = 1U<<8;  // Long Mode Enabled
constexpr uint32_t efer_mask_lma   = 1U<<9;  // Long Mode Active
constexpr uint32_t efer_mask_nxe   = 1U<<11; // No-eXecute Enabled
constexpr uint32_t efer_mask_svme  = 1U<<12; // Secure Virtual Machine Enabled
constexpr uint32_t efer_mask_lmsle = 1U<<13; // Long Mode Segment Limit Enabled
constexpr uint32_t efer_mask_ffxsr = 1U<<14; // Fast fxsave/fxrstor
constexpr uint32_t efer_mask_tce   = 1U<<15; // Translation Cache Extension

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

}  // namespace attos

#endif

#include "isr.h"
#include <stdint.h>
#include <attos/cpu.h>
#include <attos/mm.h>
#include <attos/out_stream.h>

namespace attos {

struct interrupt_gate {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;      // 0
    uint8_t  type;     // lsb->msb 4bits type, 0, 2bits dpl, 1 bit present
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
};
static_assert(sizeof(interrupt_gate)==16,"");

#pragma pack(push, 1)
struct idt_descriptor {
    uint16_t limit;
    uint64_t base;
};
static_assert(sizeof(idt_descriptor)==10,"");
#pragma pack(pop)

enum class interrupt_number : uint8_t {
    DE  = 0,  // #DE Divide by zero
    DB  = 1,  // #DB Debug exception (DR6 contains debug status)
    NMI = 2,  // NMI
    BP  = 3,  // #BP Breakpoint
    OF  = 4,  // #OF Overflow
    BR  = 5,  // #BR Bound-Range
    UD  = 6,  // #UD Invalid opcode
    NM  = 7,  // #NM Device not available
    DF  = 8,  // #DF Double fault
    TS  = 10, // #TS TSS Invalid
    NP  = 11, // #NP Segment not present
    SS  = 12, // #SS Stack exception
    GP  = 13, // #GP General protection fault
    PF  = 14, // #PF Page fault (CR2 has faulting address)
    MF  = 16, // #MF Floating point exception
    AC  = 17, // #AC Alignment check
    MC  = 18, // #MC Machine check (Info in MSRs)
    XF  = 19, // #XF SIMD exception (Info in MXCSR)
    SX  = 30, // #SX Security exception
};

bool has_error_code(interrupt_number n)
{
    return n == interrupt_number::DF ||
           n == interrupt_number::TS ||
           n == interrupt_number::NP ||
           n == interrupt_number::SS ||
           n == interrupt_number::GP ||
           n == interrupt_number::PF ||
           n == interrupt_number::AC;
}

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
    uint8_t  interrupt_no;
    uint8_t  reservered[7];
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

extern "C" void isr_common(void);
extern "C" void interrupt_service_routine(registers*);

class code_builder {
public:
    explicit code_builder(uint8_t* code) : code_(code), pos_(0) {
    }

    void push_imm8(int8_t imm) {
        constexpr uint8_t push_byte_opcode  = 0x6A;
        code_[pos_++] = push_byte_opcode;
        code_[pos_++] = static_cast<uint8_t>(imm);
    }

    void call_rel32(const void* target) {
        constexpr uint8_t call_rel32_opcode = 0xE9;
        const int64_t offset =  static_cast<const uint8_t*>(target) - (code_ + pos_ + 5);
        REQUIRE(offset >= INT32_MIN && offset <= INT32_MAX);
        code_[pos_++] = call_rel32_opcode;
        *reinterpret_cast<int32_t*>(code_ + pos_) = static_cast<int32_t>(offset);
        pos_ += 4;
    }

private:
    uint8_t* code_;
    int      pos_;
};

void interrupt_service_routine(registers*)
{
    bochs_magic();
}

extern uint8_t read_key();

void set_idt_entry(interrupt_gate& idt, void* code)
{
    const uint64_t base = virtual_address::in_current_address_space(code);
    idt.offset_low  = base & 0xffff;
    idt.selector    = cpu_manager::kernel_cs;
    idt.ist         = 0;
    idt.type        = 0x8E; // (32-bit) Interrupt gate, present
    idt.offset_mid  = (base >> 16) & 0xffff;
    idt.offset_high = base >> 32;
    idt.reserved    = 0;
}

class isr_handler_impl : public isr_handler {
public:
    isr_handler_impl() {
        __sidt(&old_idt_desc_);
        dbgout() << "[isr] Loading interrupt descriptor table.\n";
        for (int i = 0; i < idt_count; ++i) {
            const auto n = static_cast<interrupt_number>(i);
            uint8_t* const code = &isr_code_[isr_code_size * i];
            code_builder c{code};
            if (!has_error_code(n)) {
                c.push_imm8(0);
            }
            c.push_imm8(static_cast<uint8_t>(i));
            c.call_rel32(&isr_common);
            set_idt_entry(idt_[i], code);
        }
        idt_desc_.limit = sizeof(idt_)-1;
        idt_desc_.base  = virtual_address::in_current_address_space(&idt_);
        auto iv = &isr_code_[0x80*isr_code_size];
        dbgout() << "Interrupt vector 0x80: " << as_hex(virtual_address::in_current_address_space(iv)) << "\n";
        for (int i = 0; i < isr_code_size; ++i) {
            dbgout() << as_hex(iv[i]) << " ";
        }
        dbgout() << "\n";
        dbgout() << "&isr_common = " << as_hex((uint64_t)&isr_common) << "\n";
        dbgout() << "Waiting for key...\n";
        read_key();
        __lidt(&idt_desc_);
        sw_int<0x80>();
        //_enable();
    }

    isr_handler_impl(const isr_handler_impl&) = delete;
    isr_handler_impl& operator=(const isr_handler_impl&) = delete;

    ~isr_handler_impl() {
        dbgout() << "[isr] Shutting down. Restoring IDT to limit " << as_hex(old_idt_desc_.limit) << " base " << as_hex(old_idt_desc_.base) << "\n";
        _disable();
        __lidt(&old_idt_desc_);
    }

private:
    static constexpr int idt_count     = 256;
    static constexpr int isr_code_size = 9;

    interrupt_gate idt_[idt_count];
    idt_descriptor old_idt_desc_;
    idt_descriptor idt_desc_;
    uint8_t isr_code_[isr_code_size * idt_count];
};
object_buffer<isr_handler_impl> isr_handler_buffer;

owned_ptr<isr_handler, destruct_deleter> isr_init() {
    return owned_ptr<isr_handler, destruct_deleter>{isr_handler_buffer.construct().release()};
}

} // namespace attos

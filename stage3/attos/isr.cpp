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
    DE  = 0x00, // #DE Divide by zero
    DB  = 0x01, // #DB Debug exception (DR6 contains debug status)
    NMI = 0x02, // NMI
    BP  = 0x03, // #BP Breakpoint
    OF  = 0x04, // #OF Overflow
    BR  = 0x05, // #BR Bound-Range
    UD  = 0x06, // #UD Invalid opcode
    NM  = 0x07, // #NM Device not available
    DF  = 0x08, // #DF Double fault
    TS  = 0x0A, // #TS TSS Invalid
    NP  = 0x0B, // #NP Segment not present
    SS  = 0x0C, // #SS Stack exception
    GP  = 0x0D, // #GP General protection fault
    PF  = 0x0E, // #PF Page fault (CR2 has faulting address)
    MF  = 0x10, // #MF Floating point exception
    AC  = 0x11, // #AC Alignment check
    MC  = 0x12, // #MC Machine check (Info in MSRs)
    XF  = 0x13, // #XF SIMD exception (Info in MXCSR)
    SX  = 0x1E, // #SX Security exception
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
    interrupt_number interrupt_no;
    uint8_t  reservered[7];
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

extern "C" void isr_common(void);
extern "C" void interrupt_service_routine(registers&);

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

constexpr uint16_t pic1_command = 0x20;
constexpr uint16_t pic1_data    = 0x21;
constexpr uint16_t pic2_command = 0xA0;
constexpr uint16_t pic2_data    = 0xA1;

constexpr uint16_t pic_irq_mask_all = 0xFFFF;

constexpr uint8_t pic_eoi             = 0x20;

void io_wait()
{
    /*
    __asm__ __volatile__ (
        "jmp 1f\n\t"
        "1:jmp 2f\n\t"
        "2:" ::: "memory");
        */
    _mm_pause();
}

uint16_t pic_irq_mask() {
    return __inbyte(pic1_data) | (__inbyte(pic2_data) << 8);
}

void pic_irq_mask(uint16_t mask) {
    __outbyte(pic1_data, mask & 0xff);
    io_wait();
    __outbyte(pic2_data, mask >> 8);
    io_wait();
}

void pic_remap(uint8_t pic1_offset, uint8_t pic2_offset)
{
    pic_irq_mask(pic_irq_mask_all);

    constexpr uint8_t pic_read_irr        = 0x0A;       // OCW3 irq ready next CMD read
    constexpr uint8_t pic_read_isr        = 0x0B;       // OCW3 irq service next CMD read
    constexpr uint8_t pic_icw1_icw4       = 0x01;       // ICW4 (not) needed
    constexpr uint8_t pic_icw1_single     = 0x02;       // Single (cascade) mode
    constexpr uint8_t pic_icw1_interval4  = 0x04;       // Call address interval 4 (8)
    constexpr uint8_t pic_icw1_level      = 0x08;       // Level triggered (edge) mode
    constexpr uint8_t pic_icw1_init       = 0x10;       // Initialization - required!

    constexpr uint8_t pic_icw4_8086       = 0x01;       // 8086/88 (MCS-80/85) mode
    constexpr uint8_t pic_icw4_auto       = 0x02;       // Auto (normal) EOI
    constexpr uint8_t pic_icw4_buf_slave  = 0x08;       // Buffered mode/slave
    constexpr uint8_t pic_icw4_buf_master = 0x0C;       // Buffered mode/master
    constexpr uint8_t pic_icw4_sfnm       = 0x10;       // Special fully nested (not)

    // Remap PIC
    __outbyte(pic1_command, pic_icw1_init|pic_icw1_icw4);  // starts the initialization sequence (in cascade mode)
    io_wait();
    __outbyte(pic2_command, pic_icw1_init|pic_icw1_icw4);
    io_wait();
    __outbyte(pic1_data, pic1_offset);             // ICW2: Master PIC vector offset
    io_wait();
    __outbyte(pic2_data, pic2_offset+8);           // ICW2: Slave PIC vector offset
    io_wait();
    __outbyte(pic1_data, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    io_wait();
    __outbyte(pic2_data, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();
    __outbyte(pic1_data, pic_icw4_8086);
    io_wait();
    __outbyte(pic2_data, pic_icw4_8086);
    io_wait();

    pic_irq_mask(pic_irq_mask_all);
}

extern uint8_t read_key();

void interrupt_service_routine(registers& r)
{
#if 0
eflags 0x00000082: id vip vif ac vm rf nt IOPL=0 of df if tf SF zf af pf cf
#endif
    dbgout() << "interrupt_service_routine: interrupt 0x" << as_hex(r.interrupt_no);
     if (has_error_code(r.interrupt_no)) {
         dbgout() << " error_code = " << as_hex(r.error_code);
     }
     dbgout() << "\n";
#define PREG(reg, s) dbgout() << format_str(#reg).width(3) << ": " << as_hex(r.reg) << s
#define PREG2(reg1, reg2) PREG(reg1, ' '); PREG(reg2, '\n')
    PREG2(rax, rcx);
    PREG2(rdx, rbx);
    PREG2(rsp, rbp);
    PREG2(rsi, rdi);
    PREG2(r8,  r9);
    PREG2(r10, r11);
    PREG2(r12, r13);
    PREG2(r14, r15);
    PREG2(rip, rflags);
#undef PREG2
#undef PREG
    if (static_cast<uint8_t>(r.interrupt_no) == 0x80) {
        const auto key = read_key();
        dbgout() << "key = 0x" << as_hex(key) << "\n";
        return;
    }
    REQUIRE(false);
}

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
    isr_handler_impl() : old_pic_mask_(pic_irq_mask()) {
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
        __lidt(&idt_desc_);
        sw_int<0x80>();
        pic_remap(0x30, 0x38);
        _enable();
    }

    isr_handler_impl(const isr_handler_impl&) = delete;
    isr_handler_impl& operator=(const isr_handler_impl&) = delete;

    ~isr_handler_impl() {
        dbgout() << "[isr] Shutting down. Restoring IDT to limit " << as_hex(old_idt_desc_.limit) << " base " << as_hex(old_idt_desc_.base) << "\n";
        _disable();
        __lidt(&old_idt_desc_);
        bochs_magic();
        pic_remap(0x08, 0x70);
        pic_irq_mask(old_pic_mask_);
    }

private:
    static constexpr int idt_count     = 256;
    static constexpr int isr_code_size = 9;

    interrupt_gate idt_[idt_count];
    idt_descriptor old_idt_desc_;
    uint16_t       old_pic_mask_;
    idt_descriptor idt_desc_;
    uint8_t isr_code_[isr_code_size * idt_count];
};
object_buffer<isr_handler_impl> isr_handler_buffer;

owned_ptr<isr_handler, destruct_deleter> isr_init() {
    return owned_ptr<isr_handler, destruct_deleter>{isr_handler_buffer.construct().release()};
}

} // namespace attos

#include "cpu.h"
#include <attos/out_stream.h>

namespace attos {

void fatal_error(const char* file, int line, const char* detail)
{
    _disable();
    dbgout() << file << ':' << line << ": " << detail << ".\nHanging\n";
    bochs_magic();
    for (;;) {
        __halt();
    }
}

#pragma pack(push, 1)
struct gdt_descriptor {
    uint16_t limit;
    uint64_t base;
};

struct tss_header {
    uint32_t ign0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t ign1;
    uint64_t ist[7];
    uint32_t ign2;
    uint32_t ign3;
    uint16_t ign4;
    uint16_t iobase;
};
static_assert(sizeof(tss_header)==0x68,"");

struct tss : tss_header {
    static constexpr unsigned io_bitmap_bits  = 65536;
    static constexpr unsigned io_bitmap_bytes = io_bitmap_bits / 8;
    static constexpr unsigned io_bitmap_elems = io_bitmap_bytes / sizeof(uint64_t);

    // AMD24593 - AMD64 Architecture Programmer's Manual Volume 2 - System Programming, 12.2.4, pg. 336
    // An extra byte must be present after the last IOPB byte. This byte must have all bits set to 1 (0FFh).
    // This allows the processor to read two IOPB bytes each time an I/O port is accessed.
    uint64_t io_bitmap[io_bitmap_elems + 1];

    void allow_port(uint16_t port) {
        io_bitmap[port>>6] &= ~(1ULL << (port & 63));
    }
};
#pragma pack(pop)

constexpr uint64_t gdt_entry(uint16_t flags, uint32_t base, uint32_t limit) {
    return ((uint64_t)limit  & 0x0000ffff)
         | ((uint64_t)(base  & 0x00ffffff) << 16)
         | ((uint64_t)(flags & 0x0000f0ff) << 40)
         | ((uint64_t)(limit & 0x000f0000) << (48-16))
         | ((uint64_t)(base  & 0xff000000) << (56-24));
}

template<typename F>
inline F* make_fun(const char* code) {
    return (F*)(void*)code;
}

#define C_REX_W             "\x48"
#define C_OPSIZE            "\x66"

#define C_ADD_RAX_IMM8      C_REX_W "\x83\xC0"

#define C_MOV_AX_CS         C_OPSIZE "\x8C\xC8"
#define C_MOV_RAX_RSP       C_REX_W "\x89\xE0"

#define C_PUSH_IMM8         "\x6A"
#define C_PUSH_IMM32        "\x68"
#define C_PUSH_RAX          "\x50"
#define C_PUSH_RCX          "\x51"
#define C_PUSH_RDX          "\x52"
#define C_PUSH_R8           C_REX_W "\x50"
#define C_PUSH_R9           C_REX_W "\x51"
#define C_PUSH_QWORD_RAX    "\xFF\x30"
#define C_RET               "\xC3"
#define C_IRETQ             C_REX_W "\xCF"
#define C_XCHG_BX_BX        C_OPSIZE "\x87\xDB"
#define C_LTR_CX            "\x0F\x00\xD9"

void (*bochs_magic)() = make_fun<void (void)>(C_XCHG_BX_BX C_RET);
const auto read_cs = make_fun<uint16_t ()>(C_MOV_AX_CS C_RET);
const auto ltr = make_fun<void (uint16_t)>(C_LTR_CX C_RET);

// RIP, CS, EFLAGS, Old RSP, SS
const auto iretq = make_fun<void (uint64_t)>(
    C_MOV_RAX_RSP
    C_PUSH_IMM8 "\x00"      // ss
    C_ADD_RAX_IMM8 "\x08"   // 4883C008          add rax,byte +0x8
    C_PUSH_RAX              // rsp
    C_ADD_RAX_IMM8 "\xF8"   // 4883C0F8          add rax,byte -0x8
    C_PUSH_IMM8 "\x02"      // eflags (bit 1 is always set)
    C_PUSH_RCX              // cs
    C_PUSH_QWORD_RAX        // rip
    C_IRETQ
);

extern "C" void switch_to(uint64_t cs, uint64_t rip, uint64_t ss, uint64_t rsp, uint64_t flags); // crt.asm - yeah doesn't belong there...

class cpu_manager_impl : public cpu_manager, public singleton<cpu_manager_impl> {
public:
    cpu_manager_impl() : old_cs_(read_cs()) {
        dbgout() << "[cpu] Initializing. Old CS=" << as_hex(old_cs_) << "\n";

        gdt_descriptor gdt_desc = { sizeof(gdt)-1, virtual_address::in_current_address_space(&gdt) };

        constexpr uint64_t tss_limit = sizeof(tss)-1;
        constexpr uint64_t tss_type  = 0x89; // Type=64 bit TSS (available) + present
        const auto tss_base = virtual_address::in_current_address_space(&tss_);
        gdt[gdt_null]      = gdt_entry(0x0000, 0, 0x00000); // 0x00 null
        gdt[gdt_kernel_cs] = gdt_entry(0xa09a, 0, 0xfffff); // 0x08 kernel code
        gdt[gdt_kernel_ds] = gdt_entry(0x8092, 0, 0xfffff); // 0x10 kernel data
        gdt[gdt_user_ds]   = gdt_entry(0x80f2, 0, 0xfffff); // 0x18 user data
        gdt[gdt_user_cs]   = gdt_entry(0xa0fa, 0, 0xfffff); // 0x20 user code
        gdt[gdt_tss0_low]  = (tss_limit & 0xFFFF) | ((tss_base & 0xFFFFFF) << 16) | (tss_type << 40) | ((tss_limit & 0xF0000) << 32) | ((tss_base & 0xFF000000) << 32);
        gdt[gdt_tss0_high] = tss_base >> 32;

        _sgdt(&old_gdt_desc_);
        _lgdt(&gdt_desc);
        ltr(gdt_tss0_low * 8);
        iretq(kernel_cs);
    }

    ~cpu_manager_impl() {
        dbgout() << "[cpu] Shutting down. Restoring GDT to limit " << as_hex(old_gdt_desc_.limit) << " base " << as_hex(old_gdt_desc_.base) << " CS " << as_hex(old_cs_) << "\n";
        _lgdt(&old_gdt_desc_);
        iretq(old_cs_);
    }

private:
    gdt_descriptor old_gdt_desc_;
    uint16_t       old_cs_;
    tss            tss_;

    enum gdt_entries {
        gdt_null,
        gdt_kernel_cs,
        gdt_kernel_ds,
        gdt_user_ds, // Note: User data/code layout required for syscall/sysret
        gdt_user_cs,
        gdt_tss0_low,
        gdt_tss0_high,

        gdt_entries_count // must be last
    };

    static_assert(gdt_kernel_cs*8 == kernel_cs, "");
    static_assert(gdt_kernel_ds*8 == kernel_ds, "");
    static_assert(gdt_user_cs*8+3 == user_cs, "");
    static_assert(gdt_user_ds*8+3 == user_ds, "");

    uint64_t gdt[gdt_entries_count];

    void do_switch_to_context(uint64_t cs, uint64_t rip, uint64_t ss, uint64_t rsp, uint64_t flags) {
        REQUIRE(tss_.rsp0 == 0);
        REQUIRE(tss_.rsp1 == 0);
        REQUIRE(tss_.rsp2 == 0);
        tss_.rsp0 = ((uint64_t)_AddressOfReturnAddress());
        switch_to(cs, rip, ss, rsp, flags);
    }

};

object_buffer<cpu_manager_impl> cpu_manager_buffer;

owned_ptr<cpu_manager, destruct_deleter> cpu_init() {
    return owned_ptr<cpu_manager, destruct_deleter>{cpu_manager_buffer.construct().release()};
}

} // namespace attos

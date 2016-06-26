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
#pragma pack(pop)

constexpr uint64_t gdt_entry(uint16_t flags, uint32_t base, uint32_t limit) {
    return ((uint64_t)limit  & 0x0000ffff)
         | ((uint64_t)(base  & 0x00ffffff) << 16)
         | ((uint64_t)(flags & 0x0000f0ff) << 40)
         | ((uint64_t)(limit & 0x000f0000) << (48-16))
         | ((uint64_t)(base  & 0xff000000) << (56-24));
}

uint64_t gdt[] = {
    gdt_entry(0x0000, 0, 0x00000), // 0x00 null
    gdt_entry(0xa09a, 0, 0xfffff), // 0x08 kernel code
    gdt_entry(0x8092, 0, 0xfffff), // 0x10 kernel data
    //gdt_entry(0x80f2, 0, 0xfffff), // 0x18 user data
    //gdt_entry(0xa0fa, 0, 0xfffff), // 0x20 user code
    //gdt_entry(0x0000, 0, 0x00000), // 0x28 TSS0
    //gdt_entry(0x0000, 0, 0x00000), // 0x30 TSS0
};
gdt_descriptor gdt_desc = { sizeof(gdt)-1, virtual_address::in_current_address_space(&gdt) };


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

void (*bochs_magic)() = make_fun<void (void)>(C_XCHG_BX_BX C_RET);
const auto read_cs = make_fun<uint16_t ()>(C_MOV_AX_CS C_RET);

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


#if 0
template<uint8_t InterruptNo>
void sw_int() {
    static constexpr uint8_t code[] = { 0xCD, InterruptNo, 0xC3 }; // int InterruptNo; ret
    ((void (*)(void))(void*)code)();
}

00000000  668CC8            mov ax,cs
00000003  668CD8            mov ax,ds
00000006  668CC0            mov ax,es
00000009  8ED8              mov ds,eax
0000000B  8EC0              mov es,eax
0000000D  4889E0            mov rax,rsp
00000010  6A10              push byte +0x10
00000012  50                push rax
00000013  6A02              push byte +0x2
00000015  6A08              push byte +0x8
00000017  6878563412        push qword 0x12345678
0000001C  48CF              iretq
0000001E  90                nop
0000001F  51                push rcx
00000020  52                push rdx
00000021  4150              push r8
00000023  4151              push r9
00000025  55                push rbp
#endif

class cpu_manager_impl : public cpu_manager {
public:
    cpu_manager_impl() : old_cs_(read_cs()) {
        dbgout() << "[cpu] Initializing. Old CS=" << as_hex(old_cs_) << "\n";
        _sgdt(&old_gdt_desc_);
        _lgdt(&gdt_desc);
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
};

object_buffer<cpu_manager_impl> cpu_manager_buffer;

owned_ptr<cpu_manager, destruct_deleter> cpu_init() {
    return owned_ptr<cpu_manager, destruct_deleter>{cpu_manager_buffer.construct().release()};
}

} // namespace attos

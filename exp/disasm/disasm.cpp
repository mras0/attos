#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdint.h>
#include <intrin.h>
#include <assert.h>
#include <algorithm>

#include <attos/pe.h>
#include <attos/cpu.h>
#include <attos/out_stream.h>

namespace attos {

void fatal_error(const char* file, int line, const char* detail) {
    fprintf(stderr, "%s:%d: %s\n", file, line, detail);
    abort();
}

} // namespace attos

namespace attos { namespace disasm {

// Prefixes:     Up to 4 bytes, one allowed from each group (*)
// Rex prefix:   1 byte (*)
// Opcode:       1, 2 or 3 bytes
// ModR/M:       Mod 7-6, Reg/Opcode 5-3, R/M 2-0 (*)
// SIB:          Scale 7-6, Index 5-3, Base 2-0 (*)
// Displacement: 1, 2 or 4 bytes (*)
// Immediate:    1, 2 or 4 bytes (*)
// (*): Optional

#if 0
A.2.1 Codes for Addressing Method
The following abbreviations are used to document addressing methods :
A Direct address : the instruction has no ModR / M byte; the address of the operand is encoded in the instruction.
No base register, index register, or scaling factor can be applied(for example, far JMP(EA)).
B The VEX.vvvv field of the VEX prefix selects a general purpose register.
D The reg field of the ModR / M byte selects a debug register (for example, MOV(0F21, 0F23)).
F EFLAGS / RFLAGS Register.
H The VEX.vvvv field of the VEX prefix selects a 128 - bit XMM register or a 256 - bit YMM register, determined
by operand type.For legacy SSE encodings this operand does not exist, changing the instruction to
destructive form.
L The upper 4 bits of the 8 - bit immediate selects a 128 - bit XMM register or a 256 - bit YMM register, determined
by operand type. (the MSB is ignored in 32 - bit mode)
N The R / M field of the ModR / M byte selects a packed - quadword, MMX technology register.
O The instruction has no ModR / M byte.The offset of the operand is coded as a word or double word
(depending on address size attribute) in the instruction.No base register, index register, or scaling factor
can be applied(for example, MOV(A0�A3)).
P The reg field of the ModR / M byte selects a packed quadword MMX technology register.
Q A ModR / M byte follows the opcode and specifies the operand.The operand is either an MMX technology
register or a memory address.If it is a memory address, the address is computed from a segment register
and any of the following values : a base register, an index register, a scaling factor, and a displacement.
S The reg field of the ModR / M byte selects a segment register (for example, MOV(8C, 8E)).
U The R / M field of the ModR / M byte selects a 128 - bit XMM register or a 256 - bit YMM register, determined by
operand type.
X Memory addressed by the DS : rSI register pair(for example, MOVS, CMPS, OUTS, or LODS).
Y Memory addressed by the ES : rDI register pair(for example, MOVS, CMPS, INS, STOS, or SCAS).

A.2.2 Codes for Operand Type
a Two one - word operands in memory or two double - word operands in memory, depending on operand - size attribute(used only by the BOUND instruction).
c Byte or word, depending on operand - size attribute.
dq Double - quadword, regardless of operand - size attribute.
p 32 - bit, 48 - bit, or 80 - bit pointer, depending on operand - size attribute.
pd 128 - bit or 256 - bit packed double - precision floating - point data.
pi Quadword MMX technology register (for example: mm0).
qq Quad - Quadword(256 - bits), regardless of operand - size attribute.
s 6 - byte or 10 - byte pseudo - descriptor.
sd Scalar element of a 128 - bit double - precision floating data.
ss Scalar element of a 128 - bit single - precision floating data.
si Doubleword integer register (for example: eax).
y Doubleword or quadword(in 64 - bit mode), depending on operand - size attribute.
#endif


enum class addressing_mode : uint8_t {
    low_instruction_bits,
    rax,
    rcx,
    rdx,
    const1,
    C, // The reg field of the ModR / M byte selects a control register (for example, MOV(0F20, 0F22)).
    E, // A ModR / M byte follows the opcode and specifies the operand.The operand is either a general - purpose register or a memory address.If it is a memory address, the address is computed from a segment register and any of the following values : a base register, an index register, a scaling factor, a displacement.
    G, // The reg field of the ModR / M byte selects a general register (for example, AX(000)).
    I, // Immediate data : the operand value is encoded in subsequent bytes of the instruction.
    J, // The instruction contains a relative offset to be added to the instruction pointer register (for example, JMP(0E9), LOOP).
    M, // The ModR / M byte may refer only to memory(for example, BOUND, LES, LDS, LSS, LFS, LGS, CMPXCHG8B).
    R, // The R / M field of the ModR / M byte may refer only to a general register (for example, MOV(0F20 - 0F23)).
    V, // The reg field of the ModR / M byte selects a 128 - bit XMM register or a 256 - bit YMM register, determined by operand type.
    W, // A ModR / M byte follows the opcode and specifies the operand. The operand is either a 128 - bit XMM register, a 256 - bit YMM register (determined by operand type), or a memory address.If it is a memory address, the address is computed from a segment register and any of the following values : a base register, an index register, a scaling factor, and a displacement.
};

enum class operand_type : uint8_t {
    none,
    mem,
    b, // Byte, regardless of operand - size attribute.
    d, // Doubleword, regardless of operand - size attribute.
    v, // Word, doubleword or quadword(in 64 - bit mode), depending on operand - size attribute.
    w, // Word, regardless of operand - size attribute.
    q, // Quadword, regardless of operand - size attribute.
    z, // Word for 16 - bit operand - size or doubleword for 32 or 64 - bit operand - size.
    x, // dq or qq based on the operand - size attribute.
    ps, // 128 - bit or 256 - bit packed single - precision floating - point data.
};

struct instruction_operand_info {
    addressing_mode addr_mode;
    operand_type    op_type;
};

constexpr int max_operands = 2;

enum class instruction_info_type {
    normal,
    group_names,
    group
};
constexpr unsigned instruction_info_flag_f64 = 1; // The operand size is forced to a 64-bit operand size when in 64-bit mode (prefixes that change operand size are ignored for this instruction in 64-bit mode).
constexpr unsigned instruction_info_flag_d64 = 2; // When in 64-bit mode, instruction defaults to 64-bit operand size and cannot encode 32-bit operand size. 
constexpr unsigned instruction_info_flag_db66 = 4; // Hack: Require db 0x66 prefix

struct instruction_info {
    instruction_info_type type;
    unsigned flags;
    union {
        const char*             name;
        const char* const*      group_names;
        const instruction_info *group;
    };
    instruction_operand_info operands[max_operands];

    constexpr instruction_info() = default;
    constexpr instruction_info(const char* name, unsigned flags, instruction_operand_info op0=instruction_operand_info{}, instruction_operand_info op1=instruction_operand_info{})
        : type(instruction_info_type::normal), flags(flags), name(name), operands{op0, op1} {
    }

    constexpr instruction_info(const char* name, instruction_operand_info op0=instruction_operand_info{}, instruction_operand_info op1=instruction_operand_info{})
        : instruction_info{name, 0, op0, op1} {
    }

    constexpr instruction_info(const char* const* group_names, instruction_operand_info op0=instruction_operand_info{}, instruction_operand_info op1=instruction_operand_info{})
        : type(instruction_info_type::group_names), flags(0), group_names(group_names), operands{op0, op1} {
    }

    constexpr instruction_info(const instruction_info* group, instruction_operand_info op0=instruction_operand_info{}, instruction_operand_info op1=instruction_operand_info{})
        : type(instruction_info_type::group), flags(0), group(group), operands{op0, op1} {
    }
};

enum class decoded_operand_type {
    reg8,
    reg8_rex,
    reg16,
    reg32,
    reg64,
    xmmreg,
    creg,
    disp8,
    disp32,
    imm8,
    imm16,
    imm32,
    imm64,
    mem,
};

struct mem_op {
    uint8_t reg;
    uint8_t reg2;
    uint8_t reg2scale;
    int     dispbits;
    int32_t disp;
};

struct decoded_operand {
    decoded_operand_type type;
    union {
        uint8_t  reg;
        int32_t  disp;
        int64_t  imm;
        mem_op   mem;
    };
};

const uint8_t rex_b_mask = 0x1; // Extension of r/m field, base field, or opcode reg field
const uint8_t rex_x_mask = 0x2; // Extension of extension of SIB index field
const uint8_t rex_r_mask = 0x4; // Extension of ModR/M reg field
const uint8_t rex_w_mask = 0x8; // 64 Bit Operand Size

constexpr unsigned prefix_flag_lock   = 0x10;
constexpr unsigned prefix_flag_opsize = 0x20; // Operand-size override prefix
constexpr unsigned prefix_flag_rex    = 0x40;
constexpr unsigned prefix_flag_rep    = 0x80;
struct decoded_instruction {
    unsigned                prefixes;
    unsigned                unused_prefixes;
    const uint8_t*          start;
    int                     len;
    const instruction_info* info;
    int                     group;
    decoded_operand         operands[max_operands];
};

constexpr unsigned instruction_offset_0f = 256;
instruction_info instructions[512];

bool has_modrm(const instruction_info& ii)
{
    for (int i = 0; i < max_operands && ii.operands[i].op_type != operand_type::none; ++i) {
        switch (ii.operands[i].addr_mode) {
            case addressing_mode::C:
            case addressing_mode::E:
            case addressing_mode::G:
            case addressing_mode::M:
            case addressing_mode::R:
            case addressing_mode::V:
            case addressing_mode::W:
                return true;
            case addressing_mode::low_instruction_bits:
            case addressing_mode::rax:
            case addressing_mode::rcx:
            case addressing_mode::rdx:
            case addressing_mode::const1:
            case addressing_mode::I:
            case addressing_mode::J:
                break;
            default:
                assert(false);
        }
    }
    return false;
}

decoded_operand do_reg(operand_type type, uint8_t reg, bool rex_active)
{
    decoded_operand op;
    op.reg = reg;
    switch (type) {
        case operand_type::b:
            op.type = rex_active ? decoded_operand_type::reg8_rex : decoded_operand_type::reg8;
            break;
        case operand_type::w:
            assert(!rex_active);
            op.type = decoded_operand_type::reg16;
            break;
        case operand_type::v:
            op.type = rex_active ? decoded_operand_type::reg64 : decoded_operand_type::reg32;
            break;
        default:
            assert(false);
    }
    return op;
}

constexpr uint8_t modrm_mod(uint8_t modrm) { return (modrm >> 6) & 3; }
constexpr uint8_t modrm_reg(uint8_t modrm) { return (modrm >> 3) & 7; }
constexpr uint8_t modrm_rm(uint8_t modrm) { return modrm & 7; }

const char* const reg8_rex[16]  = {
    "al",  "cl",  "dl", "bl", "spl", "bpl", "sil", "dil"
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
};
const char* const reg8[16] = {
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
};

constexpr int reg_rax = 0;
constexpr int reg_rcx = 1;
constexpr int reg_rdx = 2;
constexpr int reg_rbx = 3;
constexpr int reg_rip = 16;
const char* const reg16[8] = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" };
const char* const reg32[16] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi", "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d" };
const char* const reg64[17] = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rip" };
const char* const xmmreg[16] = { "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15" };
const char* const creg[8] = { "cr0", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7" };

namespace operand_info_helpers {
constexpr auto d64 = instruction_info_flag_d64;
constexpr auto f64 = instruction_info_flag_f64;
constexpr auto db66 = instruction_info_flag_db66;
#define II(addr_mode, type) instruction_operand_info{addressing_mode::addr_mode, operand_type::type}
constexpr auto Cd = II(C, d);
constexpr auto Eb = II(E, b);
constexpr auto Ev = II(E, v);
constexpr auto Ew = II(E, w);
constexpr auto Gb = II(G, b);
constexpr auto Gv = II(G, v);
constexpr auto Ib = II(I, b);
constexpr auto Iv = II(I, v);
constexpr auto Iw = II(I, w);
constexpr auto Iz = II(I, z);
constexpr auto M  = II(M, mem);
constexpr auto Ms = II(M, mem);
constexpr auto Rd = II(R, d);
constexpr auto Jb = II(J, b);
constexpr auto Jz = II(J, z);
constexpr auto Vps = II(V, ps);
constexpr auto Vx = II(V, x);
constexpr auto Wps = II(W, ps);
constexpr auto Wx = II(W, x);

// pseduo operand types
constexpr auto R8 = II(low_instruction_bits, b);
constexpr auto R64 = II(low_instruction_bits, q);
constexpr auto rAL = II(rax, b);
constexpr auto rCL = II(rcx, b);
constexpr auto rAXz = II(rax, z);
constexpr auto rDX = II(rdx, w);
constexpr auto const1 = II(const1, b);
#undef II
};

decoded_instruction do_disasm(const uint8_t* code)
{
    static bool init = false;
    if (!init) {
        using namespace operand_info_helpers;
        instructions[0x00] = instruction_info{"add",  Eb, Gb};     // ADD r/m8, r8				
        instructions[0x01] = instruction_info{"add",  Ev, Gv};     // ADD r/m16/32/64, r16/32/64	
        instructions[0x02] = instruction_info{"add",  Gb, Eb};     // ADD r8, r/m8				
        instructions[0x03] = instruction_info{"add",  Gv, Ev};     // ADD r16/32/64, r/m16/32/64	
        instructions[0x04] = instruction_info{"add",  rAL, Ib};    // ADD AL, imm8				
        instructions[0x05] = instruction_info{"add",  rAXz, Iz};   // ADD rAX, imm16/32

        instructions[0x08] = instruction_info{"or",  Eb, Gb};      // OR r/m8, r8				
        instructions[0x09] = instruction_info{"or",  Ev, Gv};      // OR r/m16/32/64, r16/32/64	
        instructions[0x0a] = instruction_info{"or",  Gb, Eb};      // OR r8, r/m8				
        instructions[0x0b] = instruction_info{"or",  Gv, Ev};      // OR r16/32/64, r/m16/32/64	
        instructions[0x0c] = instruction_info{"or",  rAL, Ib};     // OR AL, imm8				
        instructions[0x0d] = instruction_info{"or",  rAXz, Iz};    // OR rAX, imm16/32

        instructions[0x1B] = instruction_info{"sbb",  Gv, Ev};     // SBB r16/32/64, r/m16/32/64

        instructions[0x20] = instruction_info{"and",  Eb, Gb};     // AND r/m8, r8				
        instructions[0x21] = instruction_info{"and",  Ev, Gv};     // AND r/m16/32/64, r16/32/64	
        instructions[0x22] = instruction_info{"and",  Gb, Eb};     // AND r8, r/m8				
        instructions[0x23] = instruction_info{"and",  Gv, Ev};     // AND r16/32/64, r/m16/32/64	
        instructions[0x24] = instruction_info{"and",  rAL, Ib};    // AND AL, imm8				
        instructions[0x25] = instruction_info{"and",  rAXz, Iz};   // AND rAX, imm16/32

        instructions[0x2b] = instruction_info{"sub",  Gv, Ev};     // SUB r16/32/64, r/m16/32/64

        instructions[0x30] = instruction_info{"xor",  Eb, Gb};     // XOR r/m8, r8				
        instructions[0x31] = instruction_info{"xor",  Ev, Gv};     // XOR r/m16/32/64, r16/32/64	
        instructions[0x32] = instruction_info{"xor",  Gb, Eb};     // XOR r8, r/m8				
        instructions[0x33] = instruction_info{"xor",  Gv, Ev};     // XOR r16/32/64, r/m16/32/64	
        instructions[0x34] = instruction_info{"xor",  rAL, Ib};    // XOR AL, imm8				
        instructions[0x35] = instruction_info{"xor",  rAXz, Iz};   // XOR rAX, imm16/32


        instructions[0x38] = instruction_info{"cmp",  Eb, Gb};     // CMP r/m8, r8				
        instructions[0x39] = instruction_info{"cmp",  Ev, Gv};     // CMP r/m16/32/64, r16/32/64	
        instructions[0x3a] = instruction_info{"cmp",  Gb, Eb};     // CMP r8, r/m8				
        instructions[0x3b] = instruction_info{"cmp",  Gv, Ev};     // CMP r16/32/64, r/m16/32/64	
        instructions[0x3c] = instruction_info{"cmp",  rAL, Ib};    // CMP AL, imm8				
        instructions[0x3d] = instruction_info{"cmp",  rAXz, Iz};   // CMP rAX, imm16/32

        instructions[0x50] = instruction_info{"push", R64};        // PUSH RAX/R8
        instructions[0x51] = instruction_info{"push", R64};        // PUSH RCX/R9
        instructions[0x52] = instruction_info{"push", R64};        // PUSH RDX/R10
        instructions[0x53] = instruction_info{"push", R64};        // PUSH RBX/R11
        instructions[0x54] = instruction_info{"push", R64};        // PUSH RSP/R12
        instructions[0x55] = instruction_info{"push", R64};        // PUSH RBP/R13
        instructions[0x56] = instruction_info{"push", R64};        // PUSH RSI/R14
        instructions[0x57] = instruction_info{"push", R64};        // PUSH RDI/R15

        instructions[0x58] = instruction_info{"pop",  R64};        // POP RAX/R8
        instructions[0x59] = instruction_info{"pop",  R64};        // POP RCX/R9
        instructions[0x5a] = instruction_info{"pop",  R64};        // POP RDX/R10
        instructions[0x5b] = instruction_info{"pop",  R64};        // POP RBX/R11
        instructions[0x5c] = instruction_info{"pop",  R64};        // POP RSP/R12
        instructions[0x5d] = instruction_info{"pop",  R64};        // POP RBP/R13
        instructions[0x5e] = instruction_info{"pop",  R64};        // POP RSI/R14
        instructions[0x5f] = instruction_info{"pop",  R64};        // POP RDI/R15

        instructions[0x72] = instruction_info{"jb",   Jb};         // JB rel8
        instructions[0x73] = instruction_info{"jae",  Jb};         // JAE rel8
        instructions[0x74] = instruction_info{"je",   Jb};         // JE rel8
        instructions[0x75] = instruction_info{"jne",  Jb};         // JNE rel8
        instructions[0x76] = instruction_info{"jbe",  Jb};         // JBE rel8
        instructions[0x77] = instruction_info{"ja",   Jb};         // JA rel8

        instructions[0x78] = instruction_info{"js",   Jb};         // JS rel8
        instructions[0x79] = instruction_info{"jns",  Jb};         // JNS rel8
        instructions[0x7f] = instruction_info{"jg",  Jb};          // JG rel8

        static const char* const group1[8] = { "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp" };

        instructions[0x80] = instruction_info{group1, Eb, Ib};     // Immediate Grp 1
        instructions[0x81] = instruction_info{group1, Ev, Iz};     // Immediate Grp 1
        //instructions[0x82] = instruction_info{group1, Eb, Ib};     // Immediate Grp 1
        instructions[0x83] = instruction_info{group1, Ev, Ib};     // Immediate Grp 1
        instructions[0x84] = instruction_info{"test", Eb, Gb};     // TEST r/m8, r8
        instructions[0x85] = instruction_info{"test", Ev, Gv};     // TEST r/m16/32/64, r16/32/64

        instructions[0x88] = instruction_info{"mov",  Eb, Gb};     // MOV r/m8, r8
        instructions[0x89] = instruction_info{"mov",  Ev, Gv};     // MOV r/m16/32/64, r16/32/64
        instructions[0x8a] = instruction_info{"mov",  Gb, Eb};     // MOV r8, r/m8
        instructions[0x8b] = instruction_info{"mov",  Gv, Ev};     // MOV r16/32/64, r/m16/32/64
        instructions[0x8d] = instruction_info{"lea",  Gv, M};      // LEA r16/32/64

        instructions[0x90] = instruction_info{"nop"};              // NOP (F3 90 is pause)

        instructions[0x9c] = instruction_info{"pushfq", d64/*Fv*/};// PUSHFD/Q

        instructions[0xa4] = instruction_info{"movsb", /*Yb, Xb*/};// MOVSB
        instructions[0xab] = instruction_info{"movs", /*Yv,rAX*/};// MOVSQ
        instructions[0xa8] = instruction_info{"test", rAL, Ib};    // TEST AL, imm8
        instructions[0xa9] = instruction_info{"test", rAXz, Iz};   // TEST rAX, imm16/32

        instructions[0xb0] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8
        instructions[0xb1] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8
        instructions[0xb2] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8
        instructions[0xb3] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8
        instructions[0xb4] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8
        instructions[0xb5] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8
        instructions[0xb6] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8
        instructions[0xb7] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8

        instructions[0xb8] = instruction_info{"mov",  R64, Iv};    // MOVE RAX/R8, imm16/32/64
        instructions[0xb9] = instruction_info{"mov",  R64, Iv};    // MOVE RCX/R9, imm16/32/64
        instructions[0xba] = instruction_info{"mov",  R64, Iv};    // MOVE RDX/R10, imm16/32/64
        instructions[0xbb] = instruction_info{"mov",  R64, Iv};    // MOVE RBX/R11, imm16/32/64
        instructions[0xbc] = instruction_info{"mov",  R64, Iv};    // MOVE RSP/R12, imm16/32/64
        instructions[0xbd] = instruction_info{"mov",  R64, Iv};    // MOVE RBP/R13, imm16/32/64
        instructions[0xbe] = instruction_info{"mov",  R64, Iv};    // MOVE RSI/R14, imm16/32/64
        instructions[0xbf] = instruction_info{"mov",  R64, Iv};    // MOVE RDI/R15, imm16/32/64

        static const char* const group2[8] = { "rol", "ror", "rcl", "rcr", "shl", "shr", nullptr, "sar" };
        instructions[0xc0] = instruction_info{group2, Eb, Ib};     // Shift Grp 2
        instructions[0xc1] = instruction_info{group2, Ev, Ib};     // Shift Grp 2
        instructions[0xc2] = instruction_info{"ret", Iw};          // RET imm16
        instructions[0xc3] = instruction_info{"ret"};              // RET
        instructions[0xc6] = instruction_info{"mov", Eb, Ib};      // MOV r/m8, imm8 (Grp11 - MOV)
        instructions[0xc7] = instruction_info{"mov", Ev, Iz};      // MOV r/m16/32/64, imm16/32 (Grp 11 - MOV)

        instructions[0xc8] = instruction_info{"enter", Iw, Ib};    // ENTER imm16, imm8
        instructions[0xcc] = instruction_info{"int3"};             // INT3
        instructions[0xcf] = instruction_info{"iretq"};            // IRETQ

        instructions[0xd0] = instruction_info{group2, Eb, const1}; // Shift Grp 2
        instructions[0xd1] = instruction_info{group2, Ev, const1}; // Shift Grp 2
        instructions[0xd3] = instruction_info{group2, Ev, rCL};    // Shift Grp 2

        instructions[0xe8] = instruction_info{"call", Jz};         // CALL rel16/rel32
        instructions[0xe9] = instruction_info{"jmp",  Jz};         // JMP rel16/rel32
        instructions[0xeb] = instruction_info{"jmp",  Jb};         // JMP rel8
        instructions[0xec] = instruction_info{"in", rAL, rDX };    // IN al, dx
        instructions[0xee] = instruction_info{"out", rDX, rAL };   // OUT dx, al


        instructions[0xf4] = instruction_info{"hlt"};              // HLT
        static const instruction_info group3b[8] = {
            { "test", Ib      },
            {},
            { "not",  Eb      },
            { "neg",  Eb      },
            { "mul",  rAL, Eb },
            { "imul", rAL, Eb },
            { "div",  rAL, Eb },
            { "idiv", rAL, Eb },
        };
        instructions[0xf6] = instruction_info{group3b};            // Unary Grp 3
        static const instruction_info group3z[8] = {
            { "test", Iz      },
            {},
            { "not",  Eb      },
            { "neg",  Eb      },
            { "mul",  rAL, Eb },
            { "imul", rAL, Eb },
            { "div",  rAL, Eb },
            { "idiv", rAL, Eb },
        };
        instructions[0xf7] = instruction_info{group3z};            // Unary Grp 3

        instructions[0xfa] = instruction_info{"cli"};              // CLI
        instructions[0xfb] = instruction_info{"sti"};              // STI
        instructions[0xfc] = instruction_info{"cld"};              // CLD
        instructions[0xfd] = instruction_info{"std"};              // STD
        static const instruction_info group4[8] = {
            { "inc", Eb },
            { "dec", Eb },
            {},
            {},
            {},
            {},
            {},
            {},
        };
        instructions[0xfe] = instruction_info{group4};             // INC/DEC Grp 4

        static const instruction_info group5[8] = {
            { "inc",  Ev },
            { "dec",  Ev },
            { "call", f64, Ev },
            {},//{ "call", Ep },
            {},//{ "jmp", Ev }, // f64
            {},//{"jmp", Mp },
            {},//{"push", Ev}, // d64
            {},
        };

        instructions[0xff] = instruction_info{group5};             // INC/DEC Grp 5

        // 0F XX instructions
        auto ins0f = &instructions[instruction_offset_0f];

        static const instruction_info group7[8] = {
            instruction_info{"sgdt", Ms},
            instruction_info{"sidt", Ms},
            instruction_info{"lgdt", Ms},
            instruction_info{"lidt", Ms},
            instruction_info{},
            instruction_info{},
            instruction_info{},
            instruction_info{},
        };

        ins0f[0x01] = instruction_info{group7};                    // Grp 7
        ins0f[0x20] = instruction_info{"mov", Rd, Cd};             // MOV r64, CRn
        ins0f[0x22] = instruction_info{"mov", Cd, Rd};             // MOV CRn, r64
        ins0f[0x28] = instruction_info{"movaps", Vps, Wps};        // MOVAPS xmm, xmm/m128
        ins0f[0x42] = instruction_info{"cmovb", Gv, Ev};           // CMOVb  xmm, xmm/m128
        ins0f[0x7f] = instruction_info{"movdqa", db66, Wx, Vx};    // hack
        ins0f[0x82] = instruction_info{"jb", Jz};                  // JB rel16/32
        ins0f[0x84] = instruction_info{"je", Jz};                  // JE rel16/32
        ins0f[0x85] = instruction_info{"jne", Jz};                 // JNE rel16/32
        ins0f[0x86] = instruction_info{"jbe", Jz};                 // JBE rel16/32
        ins0f[0x88] = instruction_info{"js", Jz};                  // JS rel16/32
        ins0f[0xb6] = instruction_info{"movzx", Gv, Eb};           // MOVZX r16/32/64, r/m8
        ins0f[0xb7] = instruction_info{"movzx", Gv, Ew};           // MOVZX r16/32/64, r/m16
        ins0f[0xc1] = instruction_info{"xadd", Eb, Gb};            // XADD r/m16/32/64, r16/32/64

        init = true;
    }

    decoded_instruction ins;
    ins.start = code;
    ins.len   = 0;

    auto peek_u8 = [&] { return ins.start[ins.len]; };
    auto get_u8  = [&] { return ins.start[ins.len++]; };
    auto get_u16 = [&] { auto disp = *(uint16_t*)&ins.start[ins.len]; ins.len += 2; return disp; };
    auto get_u32 = [&] { auto disp = *(uint32_t*)&ins.start[ins.len]; ins.len += 4; return disp; };
    auto get_u64 = [&] { auto disp = *(uint64_t*)&ins.start[ins.len]; ins.len += 8; return disp; };

    // Prefixes
    ins.prefixes = 0;
    auto used_prefixes = ins.prefixes;

    if (peek_u8() == 0xf0) {
        ins.prefixes |= prefix_flag_lock;
        used_prefixes = ins.prefixes;
        get_u8(); // consume
    }

    if (peek_u8() == 0xf3) {
        ins.prefixes |= prefix_flag_rep;
        used_prefixes = ins.prefixes;
        get_u8(); // consume
    }

    if (peek_u8() == 0x66) {
        ins.prefixes |= prefix_flag_opsize;
        get_u8(); // consume
    }

    uint8_t rex = 0;
    if ((peek_u8() & 0xf0) == 0x40) {
        static_assert(prefix_flag_rex == 0x40, "");
        rex = ins.start[ins.len++];
        ins.prefixes |= rex;
    }

    uint8_t instruction_byte = get_u8();
    if (instruction_byte == 0x0f) { // Two-byte instruction
        instruction_byte = get_u8();
        ins.info  = &instructions[instruction_byte+256];
    } else {
        ins.info  = &instructions[instruction_byte];
    }

    uint8_t modrm = 0;
    if (has_modrm(*ins.info) || ins.info->type != instruction_info_type::normal) {
        modrm = get_u8();
    }

    // TODO: group selection needs to take prefixes and modrm bit 7 and 6 into account...
    auto info = ins.info;
    if (ins.info->type != instruction_info_type::normal) {
        ins.group = modrm_reg(modrm);
        if (ins.info->type == instruction_info_type::group) {
            info = &info->group[ins.group];
        }
    } else {
        ins.group = -1;
    }

    if (info->flags & instruction_info_flag_f64) {
        rex |= rex_w_mask; // Hackish...
    }

    if (info->flags & instruction_info_flag_db66) {
        assert(ins.prefixes & prefix_flag_opsize);
        used_prefixes |= prefix_flag_opsize;
    }

    for (int i = 0; i < max_operands && info->operands[i].op_type != operand_type::none; ++i) {
        assert((info->flags & ~(instruction_info_flag_f64 | instruction_info_flag_db66)) == 0);

        const auto& opinfo = info->operands[i];
        auto& op = ins.operands[i];
        switch (opinfo.addr_mode) {
            case addressing_mode::low_instruction_bits:
            {
                op.reg = instruction_byte & 7;
                if (rex & rex_b_mask) {
                    used_prefixes |= prefix_flag_rex | rex_b_mask;
                    op.reg += 8;
                }
                if (opinfo.op_type == operand_type::q) {
                    op.type = decoded_operand_type::reg64;
                } else if (opinfo.op_type == operand_type::b) {
                    op.type = decoded_operand_type::reg8;
                } else {
                    assert(false);
                }
                break;
            }
            case addressing_mode::rax:
            op.reg  = reg_rax;
                if (opinfo.op_type == operand_type::b) {
                    op.type = decoded_operand_type::reg8;
                } else if (opinfo.op_type == operand_type::z) {
                    op.type = decoded_operand_type::reg32;
                } else {
                    assert(false);
                }
            break;
            case addressing_mode::rcx:
                assert(opinfo.op_type == operand_type::b);
                op.type = decoded_operand_type::reg8;
                op.reg  = reg_rcx;
            break;
            case addressing_mode::rdx:
                assert(opinfo.op_type == operand_type::w);
                op.type = decoded_operand_type::reg16;
                op.reg  = reg_rdx;
            break;
            case addressing_mode::const1:
                assert(opinfo.op_type == operand_type::b);
                op.type = decoded_operand_type::imm8;
                op.imm  = 1;
            break;
            case addressing_mode::E:
            case addressing_mode::M:
            case addressing_mode::W:
            {
                const uint8_t mod = modrm_mod(modrm);
                const uint8_t rm  = modrm_rm(modrm);
                if (mod == 3) {
                    if (ins.prefixes & prefix_flag_opsize) {
                        assert(!rex);
                        assert(opinfo.op_type == operand_type::v);
                        op.type = decoded_operand_type::reg16;
                        op.reg  = rm;
                        used_prefixes |= prefix_flag_opsize;
                    } else {
                        op = do_reg(opinfo.op_type, rm + (rex & rex_b_mask ? 8 : 0) , rex & rex_w_mask ? true : false);
                        used_prefixes |= prefix_flag_rex | rex_b_mask | rex_w_mask;
                    }
                    if (opinfo.addr_mode == addressing_mode::W) {
                        assert(opinfo.op_type == operand_type::ps);
                        op.type = decoded_operand_type::xmmreg;
                    }
                } else {
                    op.type = decoded_operand_type::mem;
                    op.mem.dispbits = 0;
                    op.mem.disp = 0;
                    op.mem.reg2 = 0xFF;
                    op.mem.reg2scale = 0;
                    const bool is_rip_relative = mod == 0 && rm == 5;

                    if (rm == 4) {
                        // SIB
                        const uint8_t sib = get_u8();
                        op.mem.reg        = (sib & 7) + (rex & rex_b_mask ? 8 : 0);
                        assert(op.mem.reg != 5);
                        op.mem.reg2       = ((sib >> 3) & 7) + (rex & rex_x_mask ? 8 : 0);
                        op.mem.reg2scale  = 1<<((sib>>6)&3);
                        if (op.mem.reg2 == 4) op.mem.reg2scale = 0;
                        used_prefixes |= prefix_flag_rex | rex_b_mask | rex_x_mask;
                    } else if (is_rip_relative) {
                        // RIP relative
                        op.mem.reg = reg_rip;
                    } else {
                        op.mem.reg = rm;
                    }
                    if (mod == 1) {
                        op.mem.dispbits = 8;
                        op.mem.disp = static_cast<int8_t>(get_u8());
                    } else if (mod == 2 || is_rip_relative) {
                        op.mem.dispbits = 32;
                        op.mem.disp = static_cast<int32_t>(get_u32());
                    }

                    if (ins.prefixes & prefix_flag_opsize) {
                        used_prefixes |= prefix_flag_opsize;
                    }
                }
                break;
            }
            case addressing_mode::G:
                if (ins.prefixes & prefix_flag_opsize) {
                    op.type = decoded_operand_type::reg16;
                    op.reg = modrm_reg(modrm);
                    used_prefixes |= prefix_flag_opsize;
                } else {
                    op = do_reg(opinfo.op_type, modrm_reg(modrm) + (rex & rex_r_mask ? 8 : 0), rex & rex_w_mask ? true : false);
                    used_prefixes |= prefix_flag_rex | rex_r_mask | rex_w_mask;
                }
                break;
            case addressing_mode::I:
                if (opinfo.op_type == operand_type::b) {
                    op.type = decoded_operand_type::imm8;
                    op.imm  = static_cast<int8_t>(get_u8());
                } else if (opinfo.op_type == operand_type::v) {
                    used_prefixes |= prefix_flag_rex | rex_w_mask;
                    if (rex & rex_w_mask) {
                        op.type = decoded_operand_type::imm64;
                        op.imm  = static_cast<int64_t>(get_u64());
                    } else {
                        op.type = decoded_operand_type::imm32;
                        op.imm  = static_cast<int32_t>(get_u32());
                    }
                } else if (opinfo.op_type == operand_type::w) {
                    op.type = decoded_operand_type::imm16;
                    op.imm  = static_cast<int32_t>(get_u16());
                } else if (opinfo.op_type == operand_type::z) {
                    op.type = decoded_operand_type::imm32;
                    op.imm  = static_cast<int32_t>(get_u32());
                } else {
                    assert(false);
                }

                break;
            case addressing_mode::J:
                if (opinfo.op_type == operand_type::b) {
                    op.type = decoded_operand_type::disp32;
                    op.disp = static_cast<int8_t>(get_u8());
                } else if (opinfo.op_type == operand_type::z) {
                    op.type = decoded_operand_type::disp32;
                    op.disp = static_cast<int32_t>(get_u32());
                } else {
                    assert(false);
                }
                break;
            case addressing_mode::C:
                assert(opinfo.op_type == operand_type::d);
                assert(!rex);
                op.type = decoded_operand_type::creg;
                op.reg = modrm_reg(modrm);
                break;
            case addressing_mode::R:
                {
                    // The R / M field of the ModR / M byte may refer only to a general register (for example, MOV(0F20 - 0F23)).
                    assert(opinfo.op_type == operand_type::d);
                    assert(!rex);
                    const uint8_t mod = modrm_mod(modrm);
                    const uint8_t rm  = modrm_rm(modrm);
                    assert(mod == 3);
                    op.type = decoded_operand_type::reg64;
                    op.reg  = rm;
                }
                break;
            case addressing_mode::V:
                assert(opinfo.op_type == operand_type::ps || opinfo.op_type == operand_type::x);
                assert(!rex);
                op.type = decoded_operand_type::xmmreg;
                op.reg = modrm_reg(modrm);
                break;
            default:
                assert(false);
        }
    }
    ins.unused_prefixes = ins.prefixes & ~used_prefixes;
    return ins;
}

} } // namespace attos::disasm


using namespace attos;
using namespace attos::pe;
using namespace attos::disasm;

void disasm_section(uint64_t virtual_address, const uint8_t* code, int maxinst)
{
    for (int count=0;count<maxinst;count++) {
        auto ins = do_disasm(code);

        dbgout() << as_hex(virtual_address) << ' ';
        for (int i = 0; i < 12; ++i) {
            if (i < ins.len) {
                dbgout() << as_hex(ins.start[i]);
            } else {
                dbgout() << "  ";
            }
        }

        const char* instruction_name;
        auto info = ins.info;
        if (info->type != instruction_info_type::normal) {
            if (info->type == instruction_info_type::group) {
                info = &info->group[ins.group];
                instruction_name = info->name;
            } else {
                assert(info->type == instruction_info_type::group_names);
                instruction_name = info->group_names[ins.group];
            }
        } else {
            instruction_name = info->name;
        }

        int iwidth = 9;
        if (ins.prefixes & prefix_flag_lock) {
            iwidth -= 5;
            dbgout() << "lock ";
        }
        if (ins.prefixes & prefix_flag_rep) {
            iwidth -= 4;
            dbgout() << "rep ";
        }

        if (!info->name) {
            dbgout() << "Unknown instruction\n";
            return;
        }

        dbgout() << format_str(instruction_name).width(iwidth > 0 ? iwidth : 0);

        for (int i = 0; i < max_operands && info->operands[i].op_type != operand_type::none; ++i) {
            dbgout() << (i ? ", " : " ");
            const auto& op = ins.operands[i];
            switch (op.type) {
                case decoded_operand_type::reg8:
                    assert(op.reg<sizeof(reg8)/sizeof(*reg8));
                    dbgout() << reg8[op.reg];
                    break;
                case decoded_operand_type::reg8_rex:
                    assert(op.reg<sizeof(reg8_rex)/sizeof(*reg8_rex));
                    dbgout() << reg8_rex[op.reg];
                    break;
                case decoded_operand_type::reg16:
                    assert(op.reg<sizeof(reg16)/sizeof(*reg16));
                    dbgout() << reg16[op.reg];
                    break;
                case decoded_operand_type::reg32:
                    assert(op.reg<sizeof(reg32)/sizeof(*reg32));
                    dbgout() << reg32[op.reg];
                    break;
                case decoded_operand_type::reg64:
                    assert(op.reg<sizeof(reg64)/sizeof(*reg64));
                    dbgout() << reg64[op.reg];
                    break;
                case decoded_operand_type::xmmreg:
                    assert(op.reg<sizeof(xmmreg)/sizeof(*xmmreg));
                    dbgout() << xmmreg[op.reg];
                    break;
                case decoded_operand_type::creg:
                    assert(op.reg<sizeof(creg)/sizeof(*creg));
                    dbgout() << creg[op.reg];
                    break;
                case decoded_operand_type::disp8:
                case decoded_operand_type::disp32:
                    dbgout() << as_hex(virtual_address + ins.len + op.disp);
                    break;
                case decoded_operand_type::imm8:
                case decoded_operand_type::imm16:
                case decoded_operand_type::imm32:
                case decoded_operand_type::imm64:
                    dbgout() << as_hex(op.imm).width(0);
                    break;
                case decoded_operand_type::mem:
                    if (op.mem.reg == reg_rip) {
                        dbgout() << "[" << as_hex(virtual_address + ins.len + op.mem.disp).width(0) << "]";
                    } else {
                        dbgout() << "[" << reg64[op.mem.reg];
                        if (op.mem.reg2scale != 0) {
                            dbgout() << "+" << reg64[op.mem.reg2];
                            if (op.mem.reg2scale > 1) {
                                dbgout() << "*" << op.mem.reg2scale;
                            }
                        }
                        if (op.mem.dispbits) {
                            if (op.mem.disp < 0) {
                                dbgout() << "-" << as_hex(-op.mem.disp).width(0);
                            } else {
                                dbgout() << "+" << as_hex(op.mem.disp).width(0);
                            }
                        }
                        dbgout() << "]";
                    }
                    break;
                default:
                    assert(false);
            }
        }
        dbgout() << "\n";

        if (ins.unused_prefixes & ~0x4F) { // Ignore unused reg prefixes
            dbgout() << "Unused prefixes:" << as_hex(ins.unused_prefixes) << "\n";
            return;
        }

        code            += ins.len;
        virtual_address += ins.len;
    }
}

extern "C" const uint8_t mainCRTStartup[];

#include <iostream>
namespace {

class attos_stream_wrapper : public attos::out_stream {
public:
    explicit attos_stream_wrapper(std::ostream& os) : os_(os) {
        attos::set_dbgout(*this);
    }
    virtual void write(const void* data, size_t n) {
        os_.write(reinterpret_cast<const char*>(data), n);
    }
private:
    std::ostream& os_;
};

} // unnamed namespace

#include <stdio.h>
#include <vector>
std::vector<uint8_t> read_file(const char* path)
{
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        std::cerr << "Could not open " << path << std::endl;
        abort();
    }
    fseek(fp, 0, SEEK_END);
    std::vector<uint8_t> v;
    v.resize(ftell(fp));
    fseek(fp, 0, SEEK_SET);
    fread(&v[0], v.size(), 1, fp);
    std::cout << "size=" << v.size() << std::endl;
    fclose(fp);
    return v;
}

int main()
{
    attos_stream_wrapper asw{std::cout};
    const auto f = read_file("../../stage3/stage3.exe");
    const auto& image = *reinterpret_cast<const IMAGE_DOS_HEADER*>(f.data());
    for (const auto section : image.nt_headers().sections()) {
        if ((section.Characteristics & IMAGE_SCN_CNT_CODE) == 0) {
            continue;
        }
        const auto offset = image.nt_headers().OptionalHeader.AddressOfEntryPoint - section.VirtualAddress;
        uint64_t virtual_address = image.nt_headers().OptionalHeader.ImageBase + section.VirtualAddress + offset;
        disasm_section(virtual_address, &f[section.PointerToRawData + offset], 10000);
    }
#if 0
    disasm_section(reinterpret_cast<uint64_t>(_ReturnAddress()), reinterpret_cast<const uint8_t*>(_ReturnAddress()), 20);
    dbgout()<<"\n";
    const uint8_t* code = &mainCRTStartup[0];
    if (code[0] == 0xE9) code += 5 + *(uint32_t*)&code[1];
    disasm_section(reinterpret_cast<uint64_t>(code), code, 52);
#endif
}

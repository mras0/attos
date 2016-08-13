#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdint.h>
#include <intrin.h>
#include <assert.h>
#include <algorithm>

#include <attos/pe.h>
#include <attos/cpu.h>
#include <attos/out_stream.h>
#include <attos/string.h>

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
F EFLAGS / RFLAGS Register.
H The VEX.vvvv field of the VEX prefix selects a 128 - bit XMM register or a 256 - bit YMM register, determined
by operand type.For legacy SSE encodings this operand does not exist, changing the instruction to
destructive form.
L The upper 4 bits of the 8 - bit immediate selects a 128 - bit XMM register or a 256 - bit YMM register, determined
by operand type. (the MSB is ignored in 32 - bit mode)
N The R / M field of the ModR / M byte selects a packed - quadword, MMX technology register.
P The reg field of the ModR / M byte selects a packed quadword MMX technology register.
Q A ModR / M byte follows the opcode and specifies the operand.The operand is either an MMX technology
register or a memory address.If it is a memory address, the address is computed from a segment register
and any of the following values : a base register, an index register, a scaling factor, and a displacement.
X Memory addressed by the DS : rSI register pair(for example, MOVS, CMPS, OUTS, or LODS).
Y Memory addressed by the ES : rDI register pair(for example, MOVS, CMPS, INS, STOS, or SCAS).

A.2.2 Codes for Operand Type
a Two one - word operands in memory or two double - word operands in memory, depending on operand - size attribute(used only by the BOUND instruction).
c Byte or word, depending on operand - size attribute.
p 32 - bit, 48 - bit, or 80 - bit pointer, depending on operand - size attribute.
pi Quadword MMX technology register (for example: mm0).
qq Quad - Quadword(256 - bits), regardless of operand - size attribute.
s 6 - byte or 10 - byte pseudo - descriptor.
si Doubleword integer register (for example: eax).
#endif


enum class addressing_mode : uint8_t {
    low_instruction_bits,
    rax,
    rcx,
    rdx,
    const1,
    C, // The reg field of the ModR / M byte selects a control register (for example, MOV(0F20, 0F22)).
    D, // The reg field of the ModR / M byte selects a debug register (for example, MOV(0F21, 0F23)).
    E, // A ModR / M byte follows the opcode and specifies the operand.The operand is either a general - purpose register or a memory address.If it is a memory address, the address is computed from a segment register and any of the following values : a base register, an index register, a scaling factor, a displacement.
    G, // The reg field of the ModR / M byte selects a general register (for example, AX(000)).
    I, // Immediate data : the operand value is encoded in subsequent bytes of the instruction.
    J, // The instruction contains a relative offset to be added to the instruction pointer register (for example, JMP(0E9), LOOP).
    M, // The ModR / M byte may refer only to memory(for example, BOUND, LES, LDS, LSS, LFS, LGS, CMPXCHG8B).
    O, // The instruction has no ModR / M byte.The offset of the operand is coded as a word or double word (depending on address size attribute) in the instruction. No base register, index register, or scaling factor can be applied(for example, MOV(A0–A3)).
    R, // The R / M field of the ModR / M byte may refer only to a general register (for example, MOV(0F20 - 0F23)).
    S, // The reg field of the ModR / M byte selects a segment register (for example, MOV(8C, 8E)).
    U, // The R / M field of the ModR / M byte selects a 128 - bit XMM register or a 256 - bit YMM register, determined by operand type.
    V, // The reg field of the ModR / M byte selects a 128 - bit XMM register or a 256 - bit YMM register, determined by operand type.
    W, // A ModR / M byte follows the opcode and specifies the operand. The operand is either a 128 - bit XMM register, a 256 - bit YMM register (determined by operand type), or a memory address.If it is a memory address, the address is computed from a segment register and any of the following values : a base register, an index register, a scaling factor, and a displacement.
};

enum class operand_type : uint8_t {
    none,
    mem,
    b, // Byte, regardless of operand - size attribute.
    ub, // UNSIGNED (hack) Byte, regardless of operand - size attribute.
    d, // Doubleword, regardless of operand - size attribute.
    dq, // Double - quadword, regardless of operand - size attribute.
    v, // Word, doubleword or quadword(in 64 - bit mode), depending on operand - size attribute.
    w, // Word, regardless of operand - size attribute.
    q, // Quadword, regardless of operand - size attribute.
    o, // An octword (128 bits), irrespective of the effective operand size
    z, // Word for 16 - bit operand - size or doubleword for 32 or 64 - bit operand - size.
    x, // dq or qq based on the operand - size attribute.
    y, // Doubleword or quadword(in 64 - bit mode), depending on operand - size attribute.
    pd, // 128 - bit or 256 - bit packed double - precision floating - point data.
    ps, // 128 - bit or 256 - bit packed single - precision floating - point data.
    ss, // Scalar element of a 128 - bit single - precision floating data.
    sd, // Scalar element of a 128 - bit double - precision floating data.
};

struct instruction_operand_info {
    addressing_mode addr_mode;
    operand_type    op_type;
};

constexpr int max_operands = 3;

enum class instruction_info_type {
    normal,
    group_names,
    group
};
constexpr unsigned instruction_info_flag_f64 = 1;          // The operand size is forced to a 64-bit operand size when in 64-bit mode (prefixes that change operand size are ignored for this instruction in 64-bit mode).
constexpr unsigned instruction_info_flag_d64 = 2;          // When in 64-bit mode, instruction defaults to 64-bit operand size and cannot encode 32-bit operand size.
constexpr unsigned instruction_info_flag_reg_group = 4;    // The group is selected based on the reg part of the following modr/m byte
constexpr unsigned instruction_info_flag_prefix_group = 8; // The group is selected based on prefix (none, 0x66, 0xF3, 0xF2)
constexpr unsigned instruction_info_flag_mem_group = 16;   // The group is selected based on whether the following modr/m byte selects memory or a register

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
    constexpr instruction_info(const char* name, unsigned flags, instruction_operand_info op0=instruction_operand_info{}, instruction_operand_info op1=instruction_operand_info{}, instruction_operand_info op2=instruction_operand_info{})
        : type(instruction_info_type::normal), flags(flags), name(name), operands{op0, op1, op2} {
    }

    constexpr instruction_info(const char* name, instruction_operand_info op0=instruction_operand_info{}, instruction_operand_info op1=instruction_operand_info{}, instruction_operand_info op2=instruction_operand_info{})
        : instruction_info{name, 0, op0, op1, op2} {
    }

    constexpr instruction_info(const char* const* group_names, instruction_operand_info op0=instruction_operand_info{}, instruction_operand_info op1=instruction_operand_info{})
        : type(instruction_info_type::group_names), flags(instruction_info_flag_reg_group), group_names(group_names), operands{op0, op1} {
    }

    constexpr instruction_info(const instruction_info* group, unsigned flags) : type(instruction_info_type::group), flags(flags), group(group), operands{} {
    }

    constexpr instruction_info(const instruction_info (&group)[4]) : instruction_info{group, instruction_info_flag_prefix_group} {
    }

    constexpr instruction_info(const instruction_info (&group)[8]) : instruction_info{group, instruction_info_flag_reg_group} {
    }

    constexpr instruction_info(const instruction_info (&group)[8 * 2]) : instruction_info{group, instruction_info_flag_reg_group | instruction_info_flag_mem_group} {
    }

    constexpr instruction_info(const instruction_info (&group)[8 * 4]) : instruction_info{group, instruction_info_flag_reg_group | instruction_info_flag_prefix_group} {
    }
};

enum class decoded_operand_type {
    reg8,
    reg8_rex,
    reg16,
    reg32,
    reg64,
    xmmreg,
    sreg,
    creg,
    dreg,
    disp8,
    disp32,
    absaddr,
    immediate,
    mem,
};

struct mem_op {
    uint8_t reg;
    uint8_t reg2;
    uint8_t reg2scale;
    int     dispbits;
    int32_t disp;
    int     bits;
};

struct decoded_operand {
    decoded_operand_type type;
    union {
        uint8_t  reg;
        int32_t  disp;
        struct {
            int64_t value;
            int     bits;
        } imm;
        mem_op   mem;
        uint64_t absaddr;
    };
};

const uint8_t rex_b_mask = 0x1; // Extension of r/m field, base field, or opcode reg field
const uint8_t rex_x_mask = 0x2; // Extension of extension of SIB index field
const uint8_t rex_r_mask = 0x4; // Extension of ModR/M reg field
const uint8_t rex_w_mask = 0x8; // 64 Bit Operand Size

// Prefix group 1
constexpr unsigned prefix_flag_lock   = 0x100;
constexpr unsigned prefix_flag_repnz  = 0x200;
constexpr unsigned prefix_flag_rep    = 0x400;
// Prefix group 2
constexpr unsigned prefix_flag_cs     = 0x800;
//constexpr unsigned prefix_flag_ss     = 0x1000;
//constexpr unsigned prefix_flag_ds     = 0x2000;
//constexpr unsigned prefix_flag_es     = 0x4000;
//constexpr unsigned prefix_flag_fs     = 0x8000;
constexpr unsigned prefix_flag_gs     = 0x10000;
// Prefix group 3
constexpr unsigned prefix_flag_opsize = 0x20000; // Operand-size override prefix
// Prefix group 4
//constexpr unsigned prefix_flag_addrsize = 0x40000; // Address-size override prefix

constexpr unsigned prefix_flag_rex    = 0x40;

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
            case addressing_mode::D:
            case addressing_mode::E:
            case addressing_mode::G:
            case addressing_mode::M:
            case addressing_mode::R:
            case addressing_mode::S:
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
            case addressing_mode::O:
                break;
            default:
                assert(false);
        }
    }
    return false;
}

constexpr uint8_t modrm_mod(uint8_t modrm) { return (modrm >> 6) & 3; }
constexpr uint8_t modrm_reg(uint8_t modrm) { return (modrm >> 3) & 7; }
constexpr uint8_t modrm_rm(uint8_t modrm) { return modrm & 7; }

const char* const reg8_rex[16]  = {
    "al",  "cl",  "dl", "bl", "spl", "bpl", "sil", "dil",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
};
const char* const reg8[16] = {
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
};

constexpr uint8_t reg_rax = 0;
constexpr uint8_t reg_rcx = 1;
constexpr uint8_t reg_rdx = 2;
constexpr uint8_t reg_rbx = 3;
constexpr uint8_t reg_rip = 16;
constexpr uint8_t reg_invalid = 255;
const char* const reg16[16]  = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di", "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"  };
const char* const reg32[16]  = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi", "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d" };
const char* const reg64[17]  = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rip" };
const char* const xmmreg[16] = { "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15" };
const char* const sreg[6]    = { "es", "cs", "ss", "ds", "fs", "gs" };
const char* const creg[16]   = { "cr0", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7", "cr8", "cr9", "cr10", "cr11", "cr12", "cr13", "cr14", "cr15" };
const char* const dreg[8]   = { "db0", "db1", "db2", "db3", "db4", "db5", "db6", "db7" };

namespace operand_info_helpers {
constexpr auto d64 = instruction_info_flag_d64;
constexpr auto f64 = instruction_info_flag_f64;
#define II(addr_mode, type) instruction_operand_info{addressing_mode::addr_mode, operand_type::type}
constexpr auto Cd = II(C, d);
constexpr auto Dd = II(D, d);
constexpr auto Eb = II(E, b);
constexpr auto Ev = II(E, v);
constexpr auto Ew = II(E, w);
constexpr auto Ey = II(E, y);
constexpr auto Ez = II(E, z);
constexpr auto Gb = II(G, b);
constexpr auto Gv = II(G, v);
constexpr auto Gy = II(G, y);
constexpr auto Ib = II(I, b);
constexpr auto Iub = II(I, ub);
constexpr auto Iv = II(I, v);
constexpr auto Iw = II(I, w);
constexpr auto Iz = II(I, z);
constexpr auto Jb = II(J, b);
constexpr auto Jz = II(J, z);
constexpr auto M  = II(M, mem);
constexpr auto Md = II(M, d);
constexpr auto Mdq = II(M, dq);
constexpr auto Ms = II(M, mem);
constexpr auto Mw = II(M, w);
constexpr auto Ob = II(O, b);
constexpr auto Ov = II(O, v);
constexpr auto Rd = II(R, d);
constexpr auto Sw = II(S, w);
constexpr auto Ux = II(U, x);
constexpr auto Vps = II(V, ps);
constexpr auto Vq = II(V, q);
constexpr auto Vsd = II(V, sd);
constexpr auto Vss = II(V, ss);
constexpr auto Vx = II(V, x);
constexpr auto Vy = II(V, y);
constexpr auto Wdq = II(W, dq);
constexpr auto Wps = II(W, ps);
constexpr auto Wpd = II(W, pd);
constexpr auto Wsd = II(W, sd);
constexpr auto Wss = II(W, ss);
constexpr auto Wx = II(W, x);

// pseduo operand types
constexpr auto R8 = II(low_instruction_bits, b);
constexpr auto R16 = II(low_instruction_bits, w);
constexpr auto R64v = II(low_instruction_bits, v);
constexpr auto R64 = II(low_instruction_bits, q);
constexpr auto rAL = II(rax, b);
constexpr auto rAX = II(rax, w);
constexpr auto rAXz = II(rax, z);
constexpr auto rCL = II(rcx, b);
constexpr auto rDX = II(rdx, w);
constexpr auto const1 = II(const1, b);
#undef II
};

decoded_operand do_reg(operand_type type, uint8_t reg, uint8_t rex)
{
    decoded_operand op;
    op.reg = reg;
    switch (type) {
        case operand_type::b:
            op.type = rex ? decoded_operand_type::reg8_rex : decoded_operand_type::reg8;
            break;
        case operand_type::w:
            op.type = decoded_operand_type::reg16;
            break;
        case operand_type::v:
        case operand_type::y:
            op.type = rex & rex_w_mask ? decoded_operand_type::reg64 : decoded_operand_type::reg32;
            break;
        case operand_type::dq:
        case operand_type::ss:
        case operand_type::sd:
        case operand_type::pd:
        case operand_type::ps:
        case operand_type::x:
            op.type = decoded_operand_type::xmmreg;
            break;
        default:
            assert(false);
    }
    return op;
}

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

        instructions[0x10] = instruction_info{"adc",  Eb, Gb};     // ADC r/m8, r8
        instructions[0x11] = instruction_info{"adc",  Ev, Gv};     // ADC r/m16/32/64, r16/32/64
        instructions[0x12] = instruction_info{"adc",  Gb, Eb};     // ADC r8, r/m8
        instructions[0x13] = instruction_info{"adc",  Gv, Ev};     // ADC r16/32/64, r/m16/32/64
        instructions[0x14] = instruction_info{"adc",  rAL, Ib};    // ADC AL, imm8
        instructions[0x15] = instruction_info{"adc",  rAXz, Iz};   // ADC rAX, imm16/32

        instructions[0x18] = instruction_info{"sbb",  Eb, Gb};     // SBB r/m8, r8
        instructions[0x19] = instruction_info{"sbb",  Ev, Gv};     // SBB r/m16/32/64, r16/32/64
        instructions[0x1a] = instruction_info{"sbb",  Gb, Eb};     // SBB r8, r/m8
        instructions[0x1b] = instruction_info{"sbb",  Gv, Ev};     // SBB r16/32/64, r/m16/32/64
        instructions[0x1c] = instruction_info{"sbb",  rAL, Ib};    // SBB AL, imm8
        instructions[0x1d] = instruction_info{"sbb",  rAXz, Iz};   // SBB rAX, imm16/32

        instructions[0x20] = instruction_info{"and",  Eb, Gb};     // AND r/m8, r8
        instructions[0x21] = instruction_info{"and",  Ev, Gv};     // AND r/m16/32/64, r16/32/64
        instructions[0x22] = instruction_info{"and",  Gb, Eb};     // AND r8, r/m8
        instructions[0x23] = instruction_info{"and",  Gv, Ev};     // AND r16/32/64, r/m16/32/64
        instructions[0x24] = instruction_info{"and",  rAL, Ib};    // AND AL, imm8
        instructions[0x25] = instruction_info{"and",  rAXz, Iz};   // AND rAX, imm16/32

        instructions[0x28] = instruction_info{"sub",  Eb, Gb};     // SUB r/m8, r8
        instructions[0x29] = instruction_info{"sub",  Ev, Gv};     // SUB r/m16/32/64, r16/32/64
        instructions[0x2a] = instruction_info{"sub",  Gb, Eb};     // SUB r8, r/m8
        instructions[0x2b] = instruction_info{"sub",  Gv, Ev};     // SUB r16/32/64, r/m16/32/64
        instructions[0x2c] = instruction_info{"sub",  rAL, Ib};    // SUB AL, imm8
        instructions[0x2d] = instruction_info{"sub",  rAXz, Iz};   // SUB rAX, imm16/32

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

        instructions[0x63] = instruction_info{"movsxd", Gv, Ez};   // MOVSXD r32/64, r/m32 (wrongly listed as Gv, Ev in intels reference!)

        instructions[0x69] = instruction_info{"imul", Gv, Ev, Iz};
        instructions[0x6b] = instruction_info{"imul", Gv, Ev, Ib};
        instructions[0x6d] = instruction_info{"ins", /*Yz, DX*/};  // INS
        instructions[0x6f] = instruction_info{"outs", /*DX, Xz*/}; // OUTS

        instructions[0x70] = instruction_info{"jo",   Jb};         // JO rel8
        instructions[0x71] = instruction_info{"jno",  Jb};         // JNO rel8
        instructions[0x72] = instruction_info{"jb",   Jb};         // JB rel8
        instructions[0x73] = instruction_info{"jae",  Jb};         // JAE rel8
        instructions[0x74] = instruction_info{"je",   Jb};         // JE rel8
        instructions[0x75] = instruction_info{"jne",  Jb};         // JNE rel8
        instructions[0x76] = instruction_info{"jbe",  Jb};         // JBE rel8
        instructions[0x77] = instruction_info{"ja",   Jb};         // JA rel8

        instructions[0x78] = instruction_info{"js",   Jb};         // JS rel8
        instructions[0x79] = instruction_info{"jns",  Jb};         // JNS rel8
        instructions[0x7c] = instruction_info{"jl",   Jb};         // JL rel8
        instructions[0x7d] = instruction_info{"jge",  Jb};         // JGE rel8
        instructions[0x7e] = instruction_info{"jle",  Jb};         // JLE rel8
        instructions[0x7f] = instruction_info{"jg",   Jb};         // JG rel8

        static const char* const group1[8] = { "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp" };

        instructions[0x80] = instruction_info{group1, Eb, Ib};     // Immediate Grp 1
        instructions[0x81] = instruction_info{group1, Ev, Iz};     // Immediate Grp 1
        //instructions[0x82] = instruction_info{group1, Eb, Ib};     // Immediate Grp 1
        instructions[0x83] = instruction_info{group1, Ev, Ib};     // Immediate Grp 1
        instructions[0x84] = instruction_info{"test", Eb, Gb};     // TEST r/m8, r8
        instructions[0x85] = instruction_info{"test", Ev, Gv};     // TEST r/m16/32/64, r16/32/64
        instructions[0x86] = instruction_info{"xchg", Eb, Gb};     // XCHG r/m8, r8
        instructions[0x87] = instruction_info{"xchg", Ev, Gv};     // XCHG r/m16/32/64, r16/32/64

        instructions[0x88] = instruction_info{"mov",  Eb, Gb};     // MOV r/m8, r8
        instructions[0x89] = instruction_info{"mov",  Ev, Gv};     // MOV r/m16/32/64, r16/32/64
        instructions[0x8a] = instruction_info{"mov",  Gb, Eb};     // MOV r8, r/m8
        instructions[0x8b] = instruction_info{"mov",  Gv, Ev};     // MOV r16/32/64, r/m16/32/64
        instructions[0x8c] = instruction_info{"mov",  Ev, Sw};     // MOV r16/32/64, Sreg
        instructions[0x8d] = instruction_info{"lea",  Gv, M};      // LEA r16/32/64, m
        instructions[0x8e] = instruction_info{"mov",  Sw, Ew};     // MOV Sreg, r/m16
        instructions[0x8f] = instruction_info{"pop",  /*d64*/f64, Ev};    // POP r/m64/16

        static const instruction_info group_90[4] = {
            { "nop"   },
            { "xchg", R16, rAX},
            { "pause" },
            {},
        };
        instructions[0x90] = instruction_info{group_90};
        instructions[0x91] = instruction_info{"xchg", R64v, rAXz };        // XCHG RCX/R9, RAX
        instructions[0x92] = instruction_info{"xchg", R64v, rAXz };        // XCHG RDX/R10, RAX
        instructions[0x93] = instruction_info{"xchg", R64v, rAXz };        // XCHG RBX/R11, RAX
        instructions[0x94] = instruction_info{"xchg", R64v, rAXz };        // XCHG RSP/R12, RAX
        instructions[0x95] = instruction_info{"xchg", R64v, rAXz };        // XCHG RBP/R13, RAX
        instructions[0x96] = instruction_info{"xchg", R64v, rAXz };        // XCHG RSI/R14, RAX
        instructions[0x97] = instruction_info{"xchg", R64v, rAXz };        // XCHG RDI/R15, RAX

        instructions[0x98] = instruction_info{"cdqe"};             // CDQE
        instructions[0x99] = instruction_info{"cdq"};              // CDQ
        instructions[0x9c] = instruction_info{"pushfq", d64/*Fv*/};// PUSHFD/Q
        instructions[0x9d] = instruction_info{"popfq", d64/*Fv*/}; // POPFD/Q

        instructions[0xa1] = instruction_info{"mov", rAXz, Ov};    // MOV rAX, moffs16/32/64
        instructions[0xa2] = instruction_info{"mov", Ob, rAL};     // MOV moffs8, rAL
        instructions[0xa4] = instruction_info{"movsb", /*Yb, Xb*/};// MOVSB
        instructions[0xa5] = instruction_info{"stos", /*Yv, Xv*/}; // MOVS
        instructions[0xa6] = instruction_info{"cmpsb", /*Xb, Yb*/};// CMPSB

        instructions[0xa8] = instruction_info{"test", rAL, Ib};    // TEST AL, imm8
        instructions[0xa9] = instruction_info{"test", rAXz, Iz};   // TEST rAX, imm16/32
        instructions[0xaa] = instruction_info{"stosb", /*Yb, AL*/};// STOSB
        instructions[0xab] = instruction_info{"stos", /*Yv,rAX*/}; // STOS
        instructions[0xae] = instruction_info{"scasb", /*AL, Yb*/};// SCASB
        instructions[0xaf] = instruction_info{"scas", /*rAX, Yv*/};// SCAS

        instructions[0xb0] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8
        instructions[0xb1] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8
        instructions[0xb2] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8
        instructions[0xb3] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8
        instructions[0xb4] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8
        instructions[0xb5] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8
        instructions[0xb6] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8
        instructions[0xb7] = instruction_info{"mov",  R8, Ib};     // MOV r8, imm8

        instructions[0xb8] = instruction_info{"mov",  R64v, Iv};    // MOVE RAX/R8, imm16/32/64
        instructions[0xb9] = instruction_info{"mov",  R64v, Iv};    // MOVE RCX/R9, imm16/32/64
        instructions[0xba] = instruction_info{"mov",  R64v, Iv};    // MOVE RDX/R10, imm16/32/64
        instructions[0xbb] = instruction_info{"mov",  R64v, Iv};    // MOVE RBX/R11, imm16/32/64
        instructions[0xbc] = instruction_info{"mov",  R64v, Iv};    // MOVE RSP/R12, imm16/32/64
        instructions[0xbd] = instruction_info{"mov",  R64v, Iv};    // MOVE RBP/R13, imm16/32/64
        instructions[0xbe] = instruction_info{"mov",  R64v, Iv};    // MOVE RSI/R14, imm16/32/64
        instructions[0xbf] = instruction_info{"mov",  R64v, Iv};    // MOVE RDI/R15, imm16/32/64

        static const char* const group2[8] = { "rol", "ror", "rcl", "rcr", "shl", "shr", nullptr, "sar" };
        instructions[0xc0] = instruction_info{group2, Eb, Iub};    // Shift Grp 2
        instructions[0xc1] = instruction_info{group2, Ev, Iub};    // Shift Grp 2
        instructions[0xc2] = instruction_info{"ret", Iw};          // RET imm16
        instructions[0xc3] = instruction_info{"ret"};              // RET
        instructions[0xc6] = instruction_info{"mov", Eb, Ib};      // MOV r/m8, imm8 (Grp11 - MOV)
        instructions[0xc7] = instruction_info{"mov", Ev, Iz};      // MOV r/m16/32/64, imm16/32 (Grp 11 - MOV)

        instructions[0xc8] = instruction_info{"enter", Iw, Ib};    // ENTER imm16, imm8
        instructions[0xcc] = instruction_info{"int3"};             // INT3
        instructions[0xcd] = instruction_info{"int", Ib};          // INT3 imm8
        instructions[0xcf] = instruction_info{"iretq"};            // IRETQ

        instructions[0xd0] = instruction_info{group2, Eb, const1}; // Shift Grp 2
        instructions[0xd1] = instruction_info{group2, Ev, const1}; // Shift Grp 2
        instructions[0xd2] = instruction_info{group2, Eb, rCL};    // Shift Grp 2
        instructions[0xd3] = instruction_info{group2, Ev, rCL};    // Shift Grp 2

        instructions[0xe4] = instruction_info{"in", rAL, Ib};      // IN al, imm8

        instructions[0xe8] = instruction_info{"call", Jz};         // CALL rel16/rel32
        instructions[0xe9] = instruction_info{"jmp",  Jz};         // JMP rel16/rel32
        instructions[0xeb] = instruction_info{"jmp",  Jb};         // JMP rel8
        instructions[0xec] = instruction_info{"in", rAL, rDX };    // IN al, dx
        instructions[0xed] = instruction_info{"in", rAXz, rDX };   // IN eAX, dx
        instructions[0xee] = instruction_info{"out", rDX, rAL };   // OUT dx, eAX
        instructions[0xef] = instruction_info{"out",  rDX, rAXz};  // JMP rel16/rel32


        instructions[0xf4] = instruction_info{"hlt"};              // HLT
        static const instruction_info group3b[8] = {
            { "test", Eb, Ib  },
            {},
            { "not",  Eb      },
            { "neg",  Eb      },
            { "mul",  Eb      },
            { "imul", Eb      },
            { "div",  Eb      },
            { "idiv", Eb      },
        };
        instructions[0xf6] = instruction_info{group3b};            // Unary Grp 3
        static const instruction_info group3z[8] = {
            { "test", Ev, Iz },
            {},
            { "not",  Ev     },
            { "neg",  Ev     },
            { "mul",  Ev     }, // Not tested
            { "imul", Ev     }, // Not tested
            { "div",  Ev     }, // Not tested
            { "idiv", Ev     }, // Not tested
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
            { "jmp", f64, Ev },
            {},//{"jmp", Mp },
            {"push", /*d64*/f64, Ev},
            {},
        };

        instructions[0xff] = instruction_info{group5};             // INC/DEC Grp 5

        // 0F XX instructions
        auto ins0f = &instructions[instruction_offset_0f];

        static const instruction_info group6[8] = {
            instruction_info{ "sldt", Mw },
            instruction_info{ "str",  Mw },
            instruction_info{ "lldt", Ew },
            instruction_info{ "ltr",  Ew },
            instruction_info{},
            instruction_info{},
            instruction_info{},
            instruction_info{},
        };
        ins0f[0x00] = instruction_info{group6};                    // Grp 6
        static const instruction_info group7[16] = {
            /*        /0                            /1                            /2                            /3                            /4                  /5                  /6                  /7 */
            /* mem */ instruction_info{"sgdt", Ms}, instruction_info{"sidt", Ms}, instruction_info{"lgdt", Ms}, instruction_info{"lidt", Ms}, instruction_info{}, instruction_info{}, instruction_info{}, instruction_info{},
            /* 11b */ instruction_info{},           instruction_info{},           instruction_info{"xgetbv"},   instruction_info{},           instruction_info{}, instruction_info{}, instruction_info{}, instruction_info{},
        };
        ins0f[0x01] = instruction_info{group7};                    // Grp 7
        ins0f[0x05] = instruction_info{"syscall"};
        ins0f[0x07] = instruction_info{"sysret"};

        ins0f[0x0d] = instruction_info{"prefetchw", Eb/*Ev*/};

        static const instruction_info group_0f_10[4] = {
            {"movups", Vps, Wps}, // MOVUPS xmm, xmm/m128
            {},
            {"movss", Vx, Wss},
            {"movsd", Vx, Wsd},
        };
        ins0f[0x10] = instruction_info{group_0f_10};
        static const instruction_info group_0f_11[4] = {
            {"movups", Wps, Vps}, // MOVUPS xmm/m128, xmm
            {},
            {"movss", Wss, Vss},
            {"movsd", Wsd, Vsd},
        };
        ins0f[0x11] = instruction_info{group_0f_11};

        static const instruction_info group16[8] = {
            {"prefetchnta", /*M*/Eb},
            {},
            {},
            {},
            {},
            {},
            {},
            {},
        };
        ins0f[0x18] = instruction_info{group16};                   // Grp 16
        ins0f[0x1f] = instruction_info{"nop", Ev};                 // Hintable NOP

        ins0f[0x20] = instruction_info{"mov", Rd, Cd};             // MOV r64, CRn
        ins0f[0x22] = instruction_info{"mov", Cd, Rd};             // MOV CRn, r64
        ins0f[0x23] = instruction_info{"mov", Dd, Rd};             // MOV DRn, r64
        ins0f[0x28] = instruction_info{"movaps", Vps, Wps};        // MOVAPS xmm, xmm/m128
        static const instruction_info group_0f_29[4] = {
            {"movaps", Wps, Vps},
            {},
            {},
            {},
        };
        ins0f[0x29] = instruction_info{group_0f_29};
        static const instruction_info group_0f_2a[4] = {
            {},
            {},
            {"cvtsi2ss", Vss, Ey},
            {"cvtsi2sd", Vsd, Ey},
        };
        ins0f[0x2a] = instruction_info{group_0f_2a};
        static const instruction_info group_0f_2c[4] = {
            {},
            {},
            {"cvttss2si", Gy, Wss},
            {"cvttsd2si", Gy, Wsd},
        };
        ins0f[0x2c] = instruction_info{group_0f_2c};
        static const instruction_info group_0f_2f[4] = {
            {"comiss", Vss, Wss},
            {"comisd", Vsd, Wsd},
            {},
            {},
        };
        ins0f[0x2f] = instruction_info{group_0f_2f};

        ins0f[0x30] = instruction_info{"wrmsr"};
        ins0f[0x31] = instruction_info{"rdtsc"};
        ins0f[0x32] = instruction_info{"rdmsr"};
        ins0f[0x33] = instruction_info{"rdpmc"};
        ins0f[0x34] = instruction_info{"sysenter"};
        ins0f[0x35] = instruction_info{"sysexit"};

        ins0f[0x40] = instruction_info{"cmovo",   Gv, Ev};         // CMOVO r16/32/64, r/m16/32/64
        ins0f[0x41] = instruction_info{"cmovno",  Gv, Ev};         // CMOVNO r16/32/64, r/m16/32/64
        ins0f[0x42] = instruction_info{"cmovb",   Gv, Ev};         // CMOVB r16/32/64, r/m16/32/64
        ins0f[0x43] = instruction_info{"cmovae",  Gv, Ev};         // CMOVAE r16/32/64, r/m16/32/64
        ins0f[0x44] = instruction_info{"cmove",   Gv, Ev};         // CMOVE r16/32/64, r/m16/32/64
        ins0f[0x45] = instruction_info{"cmovne",  Gv, Ev};         // CMOVNE r16/32/64, r/m16/32/64
        ins0f[0x46] = instruction_info{"cmovbe",  Gv, Ev};         // CMOVBE r16/32/64, r/m16/32/64
        ins0f[0x47] = instruction_info{"cmova",   Gv, Ev};         // CMOVA r16/32/64, r/m16/32/64

        ins0f[0x48] = instruction_info{"cmovs",   Gv, Ev};         // CMOVS r16/32/64, r/m16/32/64
        ins0f[0x49] = instruction_info{"cmovns",  Gv, Ev};         // CMOVNS r16/32/64, r/m16/32/64
        ins0f[0x4c] = instruction_info{"cmovl",   Gv, Ev};         // CMOVL r16/32/64, r/m16/32/64
        ins0f[0x4d] = instruction_info{"cmovge",  Gv, Ev};         // CMOVGE r16/32/64, r/m16/32/64
        ins0f[0x4e] = instruction_info{"cmovle",  Gv, Ev};         // CMOVLE r16/32/64, r/m16/32/64
        ins0f[0x4f] = instruction_info{"cmovg",   Gv, Ev};         // CMOVG r16/32/64, r/m16/32/64

        static const instruction_info group_0f_54[4] = {
            {"andps",Vps,Wps},
            {},
            {},
            {},
        };
        ins0f[0x54] = instruction_info{group_0f_54};
        static const instruction_info group_0f_57[4] = {
            {"xorps",Vps,Wps},
            {},
            {},
            {},
        };
        ins0f[0x57] = instruction_info{group_0f_57};

        static const instruction_info group_0f_58[4] = {
            {},
            {},
            {},
            {"addsd", Vsd, Wsd },
        };
        ins0f[0x58] = instruction_info{group_0f_58};
        static const instruction_info group_0f_59[4] = {
            {},
            {},
            {"mulss", Vss, Wss},
            {"mulsd", Vsd, Wsd},
        };
        ins0f[0x59] = instruction_info{group_0f_59};
        static const instruction_info group_0f_5a[4] = {
            {},
            {},
            {"cvtss2sd", Vx, Wx},
            {"cvtsd2ss", Vss, Wsd},
        };
        ins0f[0x5a] = instruction_info{group_0f_5a};
        static const instruction_info group_0f_5b[4] = {
            {"cvtdq2ps", Vps, Wdq},
            {},
            {},
            {},
        };
        ins0f[0x5b] = instruction_info{group_0f_5b};
        static const instruction_info group_0f_5c[4] = {
            {},
            {},
            {"subss", Vss, Wss},
            {"subsd", Vsd, Wsd},
        };
        ins0f[0x5c] = instruction_info{group_0f_5c};
        static const instruction_info group_0f_5e[4] = {
            {},
            {},
            {"divss", Vss, Wss},
            {"divsd", Vsd, Wsd},
        };
        ins0f[0x5e] = instruction_info{group_0f_5e};

        static const instruction_info group_0f_6e[4] = {
            {},
            {"movd/q", Vy, Ey},
            {},
            {},
        };
        ins0f[0x6e] = instruction_info{group_0f_6e};
        static const instruction_info group_0f_6f[4] = {
            {},
            {"movdqa", Vx, Wx},
            {"movdqu", Vx, Wx},
            {},
        };
        ins0f[0x6f] = instruction_info{group_0f_6f};

        static const instruction_info group_0f_73[8 * 4] = {
            /*       /0  /1  /2  /3                  /4  /5  /6  /7 */
            /*    */ {}, {}, {}, {},                 {}, {}, {}, {},
            /* 66 */ {}, {}, {}, {"psrldq", Ux, Ib}, {}, {}, {}, {},
            /* F3 */ {}, {}, {}, {},                 {}, {}, {}, {},
            /* F2 */ {}, {}, {}, {},                 {}, {}, {}, {},
        };
        ins0f[0x73] = instruction_info{group_0f_73};
        static const instruction_info group_0f_7e[4] = {
            {},
            {"movd/q",Ey,Vq},
            {},
            {},
        };
        ins0f[0x7e] = instruction_info{group_0f_7e};
        static const instruction_info group_0f_7f[4] = {
            {}, //{ "movq", Pq, Qq },
            {"movdqa", Wx, Vx},
            {"movdqu", Wx, Vx},
            {},
        };
        ins0f[0x7f] = instruction_info{group_0f_7f};

        ins0f[0x80] = instruction_info{"jo",  Jz};                 // JO rel16/32
        ins0f[0x81] = instruction_info{"jno", Jz};                 // JNO rel16/32
        ins0f[0x82] = instruction_info{"jb",  Jz};                 // JB rel16/32
        ins0f[0x83] = instruction_info{"jae", Jz};                 // JAE rel16/32
        ins0f[0x84] = instruction_info{"je",  Jz};                 // JE rel16/32
        ins0f[0x85] = instruction_info{"jne", Jz};                 // JNE rel16/32
        ins0f[0x86] = instruction_info{"jbe", Jz};                 // JBE rel16/32
        ins0f[0x87] = instruction_info{"ja",  Jz};                 // JA rel16/32

        ins0f[0x88] = instruction_info{"js",   Jz};                // JS rel16/32
        ins0f[0x89] = instruction_info{"jns",  Jz};                // JNS rel16/32
        ins0f[0x8c] = instruction_info{"jl",   Jz};                // JL rel16/32
        ins0f[0x8d] = instruction_info{"jge",  Jz};                // JGE rel16/32
        ins0f[0x8e] = instruction_info{"jle",  Jz};                // JLE rel16/32
        ins0f[0x8f] = instruction_info{"jg",   Jz};                // JG rel16/32

        ins0f[0x90] = instruction_info{"seto",   Eb};              // SETO r/m8
        ins0f[0x91] = instruction_info{"setno",  Eb};              // SETNO r/m8
        ins0f[0x92] = instruction_info{"setb",   Eb};              // SETB r/m8
        ins0f[0x93] = instruction_info{"setae",  Eb};              // SETAE r/m8
        ins0f[0x94] = instruction_info{"sete",   Eb};              // SETE r/m8
        ins0f[0x95] = instruction_info{"setne",  Eb};              // SETNE r/m8
        ins0f[0x96] = instruction_info{"setbe",  Eb};              // SETBE r/m8
        ins0f[0x97] = instruction_info{"seta",   Eb};              // SETA r/m8

        ins0f[0x98] = instruction_info{"sets",   Eb};              // SETS r/m8
        ins0f[0x99] = instruction_info{"setns",  Eb};              // SETNS r/m8
        ins0f[0x9c] = instruction_info{"setl",   Eb};              // SETL r/m8
        ins0f[0x9d] = instruction_info{"setge",  Eb};              // SETGE r/m8
        ins0f[0x9e] = instruction_info{"setle",  Eb};              // SETLE r/m8
        ins0f[0x9f] = instruction_info{"setg",   Eb};              // SETG r/m8

        ins0f[0xa2] = instruction_info{"cpuid"};                   // CPUID
        ins0f[0xa3] = instruction_info{"bt", Ev, Gv};              // BT r/m16/32/64, r16/32/64

        ins0f[0xab] = instruction_info{"bts", Ev, Gv};             // BTS r/m16/32/64, r16/32/64

        static const instruction_info group15[16] = {
            /*        /0              /1              /2               /3               /4  /5  /6          /7 */
            /* mem */ {"fxsave",  M}, {"fxrstor", M}, {"ldmxcsr", Md}, {"stmxcsr", Md}, {}, {}, {},         {},
            /* 11b */ {},             {},             {},              {},              {}, {}, {"mfence"}, {},
        };
        ins0f[0xae] = instruction_info{group15};
        ins0f[0xaf] = instruction_info{"imul", Gv, Ev};            // IMUL r16/32/64, r/m16/32/64

        ins0f[0xb0] = instruction_info{"cmpxchg", Eb, Gb};
        ins0f[0xb1] = instruction_info{"cmpxchg", Ev, Gv};
        ins0f[0xb3] = instruction_info{"btr", Ev, Gv};             // BTR r/m16/32/64, r16/32/64
        ins0f[0xb6] = instruction_info{"movzx", Gv, Eb};           // MOVZX r16/32/64, r/m8
        ins0f[0xb7] = instruction_info{"movzx", Gv, Ew};           // MOVZX r16/32/64, r/m16

        static const char* const group8[8] = { nullptr, nullptr, nullptr, nullptr, "bt", "bts", "btr", "btc" };
        ins0f[0xba] = instruction_info{group8, Ev, Ib};            // Grp 8
        ins0f[0xbc] = instruction_info{"bsf", Gv, Ev};             // BSF r16/32/64, r/m16/32/64
        ins0f[0xbd] = instruction_info{"bsr", Gv, Ev};             // BSR r16/32/64, r/m16/32/64
        ins0f[0xbe] = instruction_info{"movsx", Gv, Eb};           // MOVSX r16/32/64, r/m8
        ins0f[0xbf] = instruction_info{"movsx", Gv, Ew};           // MOVSX r16/32/64, r/m16

        ins0f[0xc0] = instruction_info{"xadd", Eb, Gb};            // XADD r/m16/32/64, r16/32/64
        ins0f[0xc1] = instruction_info{"xadd", Ev, Gv};            // XADD r/m16/32/64, r16/32/64
        static const instruction_info group9[8 * 4] = {
            /*       /0  /1                   /2  /3  /4  /5  /6  /7 */
            /*    */ {}, {"cmpxchg16b", Mdq}, {}, {}, {}, {}, {}, {},
            /* 66 */ {},                      {}, {}, {}, {}, {}, {}, {},
            /* F3 */ {},                      {}, {}, {}, {}, {}, {}, {},
            /* F2 */ {},                      {}, {}, {}, {}, {}, {}, {},
        };
        ins0f[0xc7] = instruction_info{group9};                    // Grp9

        ins0f[0xc8] = instruction_info{"bswap", R64v};             // BSWAP RAX/R8
        ins0f[0xc9] = instruction_info{"bswap", R64v};             // BSWAP RCX/R9
        ins0f[0xca] = instruction_info{"bswap", R64v};             // BSWAP RDX/R10
        ins0f[0xcb] = instruction_info{"bswap", R64v};             // BSWAP RBX/R11
        ins0f[0xcc] = instruction_info{"bswap", R64v};             // BSWAP RSP/R12
        ins0f[0xcd] = instruction_info{"bswap", R64v};             // BSWAP RBP/R13
        ins0f[0xce] = instruction_info{"bswap", R64v};             // BSWAP RSI/R14
        ins0f[0xcf] = instruction_info{"bswap", R64v};             // BSWAP RDI/R15

        static const instruction_info group_0f_e6[4] = {
            {},
            {},
            {"cvtdq2pd", Vx, Wpd},
            {},
        };
        ins0f[0xe6] = instruction_info{group_0f_e6};
        static const instruction_info group_0f_ef[4] = {
            {}, //{"pxor", Pq, Qq},
            {"pxor", Vx, Wx},
            {},
            {},
        };
        ins0f[0xef] = instruction_info{group_0f_ef};

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

    uint8_t rex = 0;
    for (;;) {
        const uint8_t b = peek_u8();
        if (b == 0xf0) {
            ins.prefixes |= prefix_flag_lock;
        } else if (b == 0xf2) {
            ins.prefixes |= prefix_flag_repnz;
        } else if (b == 0xf3) {
            ins.prefixes |= prefix_flag_rep;
        } else if (b == 0x2e) {
            ins.prefixes |= prefix_flag_cs;
        } else if (b == 0x65) {
            ins.prefixes |= prefix_flag_gs;
        } else if (b == 0x66) {
            ins.prefixes |= prefix_flag_opsize;
        } else if ((b & 0xf0) == 0x40) {
            static_assert(prefix_flag_rex == 0x40, "");
            rex = b;
            ins.prefixes |= rex;
        } else {
            break;
        }
        get_u8(); // consume
    }

    uint8_t instruction_byte = get_u8();
    if (instruction_byte == 0x0f) { // Two-byte instruction
        instruction_byte = get_u8();
        ins.info  = &instructions[instruction_byte+256];
    } else {
        ins.info  = &instructions[instruction_byte];
    }

    bool modrm_fetched = false;
    uint8_t modrm = 0;

    if ((ins.info->flags & instruction_info_flag_reg_group) || has_modrm(*ins.info)) {
        modrm = get_u8();
        modrm_fetched = true;
    }

    auto info = ins.info;
    if (ins.info->type != instruction_info_type::normal) {
        const auto group_flags = info->flags & (instruction_info_flag_prefix_group | instruction_info_flag_reg_group | instruction_info_flag_mem_group);

        auto get_prefix_group = [&]() {
            assert(group_flags & instruction_info_flag_prefix_group);
            if (ins.prefixes & prefix_flag_opsize) {
                used_prefixes |= prefix_flag_opsize;
                ins.prefixes &= ~prefix_flag_opsize; // We consume the flag as part of the instruction
                return 1;
            } else if (ins.prefixes & prefix_flag_rep) {
                used_prefixes |= prefix_flag_rep;
                ins.prefixes &= ~prefix_flag_rep; // We consume the flag as part of the instruction
                return 2;
            } else if (ins.prefixes & prefix_flag_repnz) {
                used_prefixes |= prefix_flag_repnz;
                ins.prefixes &= ~prefix_flag_repnz; // We consume the flag as part of the instruction
                return 3;
            } else {
                // TODO: return 3 when F2 prefix present
                return 0;
            }
        };

        if (group_flags == instruction_info_flag_prefix_group) {
            assert(ins.info->type == instruction_info_type::group);
            ins.group = get_prefix_group();
            info = &info->group[ins.group];
        } else {
            if (group_flags == instruction_info_flag_reg_group) {
                ins.group = modrm_reg(modrm);
                if (ins.info->type == instruction_info_type::group) {
                    info = &info->group[ins.group];
                }
            } else if (group_flags == (instruction_info_flag_reg_group | instruction_info_flag_prefix_group)) {
                // Hack for 0f c7 [cmpxchg16b]
                assert((modrm_mod(modrm) == 3 || ins.info == &instructions[256+0xc7]) && "TODO: group selection needs modrm bit 7 and 6 into account...");
                assert(ins.info->type == instruction_info_type::group);
                ins.group = modrm_reg(modrm) + get_prefix_group() * 8;
                info = &info->group[ins.group];
            } else {
                assert(group_flags == (instruction_info_flag_reg_group | instruction_info_flag_mem_group));
                assert(ins.info->type == instruction_info_type::group);
                assert((ins.info != &instructions[256+0x01] || modrm_mod(modrm) != 3 || modrm_rm(modrm) == 0) && "TODO: OF 01 Needs to take modrm bits 2, 1, 0 into account...");
                ins.group = modrm_reg(modrm) + (modrm_mod(modrm) == 3) * 8;
                info = &info->group[ins.group];
            }
        }
    } else {
        ins.group = -1;
    }

    if (!modrm_fetched && has_modrm(*info)) {
        modrm = get_u8();
        modrm_fetched = true;
    }

    bool restore_rex_w_mask = false;
    if (info->flags & instruction_info_flag_f64) {
        // HACK
        restore_rex_w_mask = (rex & rex_w_mask) != 0;
        rex |= rex_w_mask;
    }

    for (int i = 0; i < max_operands && info->operands[i].op_type != operand_type::none; ++i) {
        assert((info->flags & ~(instruction_info_flag_f64 | instruction_info_flag_reg_group | instruction_info_flag_prefix_group)) == 0);

        const auto& opinfo = info->operands[i];
        auto& op = ins.operands[i];


        auto do_imm = [&op](uint8_t bits, int64_t value) {
            op.type = decoded_operand_type::immediate;
            op.imm.value = value;
            op.imm.bits = bits;
        };
        auto do_imm8  = [&] { do_imm(8, static_cast<int8_t>(get_u8())); };
        auto do_imm16 = [&] { do_imm(16, static_cast<int16_t>(get_u16())); };
        auto do_imm32 = [&] { do_imm(32, static_cast<int32_t>(get_u32())); };
        auto do_imm64 = [&] { do_imm(64, static_cast<int64_t>(get_u64())); };

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
                } else if (opinfo.op_type == operand_type::v) {
                    if (rex & rex_w_mask) {
                        op.type = decoded_operand_type::reg64;
                        used_prefixes |= prefix_flag_rex | rex_w_mask;
                    } else {
                        if (ins.prefixes & prefix_flag_opsize) {
                            op.type = decoded_operand_type::reg16;
                            used_prefixes |= prefix_flag_opsize;
                        } else {
                            op.type = decoded_operand_type::reg32;
                        }
                    }
                } else if (opinfo.op_type == operand_type::b) {
                    op.type = rex ? decoded_operand_type::reg8_rex : decoded_operand_type::reg8;
                    used_prefixes |= prefix_flag_rex;
                } else if (opinfo.op_type == operand_type::w) {
                    op.type = decoded_operand_type::reg16;
                } else {
                    assert(false);
                }
                break;
            }
            case addressing_mode::rax:
                op.reg  = reg_rax;
                if (opinfo.op_type == operand_type::b) {
                    op.type = decoded_operand_type::reg8;
                } else if (opinfo.op_type == operand_type::w) {
                    op.type = decoded_operand_type::reg16;
                } else if (opinfo.op_type == operand_type::z) {
                    if (ins.prefixes & prefix_flag_opsize) {
                        assert(!(rex & rex_w_mask));
                        op.type = decoded_operand_type::reg16;
                        used_prefixes |= prefix_flag_opsize;
                    } else {
                        op.type = rex & rex_w_mask ? decoded_operand_type::reg64 : decoded_operand_type::reg32;
                        used_prefixes |= prefix_flag_rex | rex_w_mask;
                    }
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
                do_imm(1, 1);
            break;
            case addressing_mode::E:
            case addressing_mode::M:
            case addressing_mode::W:
            {
                const uint8_t mod = modrm_mod(modrm);
                const uint8_t rm  = modrm_rm(modrm);
                if (mod == 3) {
                    assert(opinfo.addr_mode != addressing_mode::M);
                    if (ins.prefixes & prefix_flag_opsize) {
                        assert(opinfo.op_type == operand_type::v || opinfo.op_type == operand_type::b || opinfo.op_type == operand_type::w);
                        op.type = decoded_operand_type::reg16;
                        op.reg  = rm;
                        if (rex & rex_b_mask) {
                            op.reg += 8;
                            used_prefixes |= prefix_flag_rex | rex_b_mask;
                        }
                        used_prefixes |= prefix_flag_opsize;
                    } else {
                        const uint8_t reg = rm + (rex & rex_b_mask ? 8 : 0);
                        used_prefixes |= prefix_flag_rex | rex_b_mask | rex_w_mask;
                        if (opinfo.op_type == operand_type::z) {
                            op.reg = reg;
                            if (ins.prefixes & prefix_flag_opsize) {
                                op.type = decoded_operand_type::reg16;
                                used_prefixes |= prefix_flag_opsize;
                            } else {
                                op.type = decoded_operand_type::reg32;
                            }
                        } else {
                            op = do_reg(opinfo.op_type, reg, rex);
                        }
                    }
                    if (opinfo.addr_mode == addressing_mode::W) {
                        assert(opinfo.op_type == operand_type::pd || opinfo.op_type == operand_type::ps || opinfo.op_type == operand_type::x || opinfo.op_type == operand_type::dq || opinfo.op_type == operand_type::ss || opinfo.op_type == operand_type::sd);
                        assert(op.type == decoded_operand_type::xmmreg);
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
                        if (mod == 0 && (sib & 7) == 5) {
                            op.mem.reg = reg_invalid;
                        } else {
                            op.mem.reg = (sib & 7) + (rex & rex_b_mask ? 8 : 0);
                        }
                        op.mem.reg2       = ((sib >> 3) & 7) + (rex & rex_x_mask ? 8 : 0);
                        op.mem.reg2scale  = 1<<((sib>>6)&3);
                        if (op.mem.reg2 == 4) op.mem.reg2scale = 0;
                        used_prefixes |= prefix_flag_rex | rex_b_mask | rex_x_mask;
                    } else if (is_rip_relative) {
                        // RIP relative
                        op.mem.reg = reg_rip;
                    } else {
                        op.mem.reg = rm;
                        if (rex & rex_b_mask) {
                            op.mem.reg += 8;
                            used_prefixes |= prefix_flag_rex | rex_b_mask;
                        }
                    }
                    if (mod == 1) {
                        op.mem.dispbits = 8;
                        op.mem.disp = static_cast<int8_t>(get_u8());
                    } else if (mod == 2 || is_rip_relative ||op.mem.reg == reg_invalid) {
                        op.mem.dispbits = 32;
                        op.mem.disp = static_cast<int32_t>(get_u32());
                    }
                    if (opinfo.addr_mode == addressing_mode::W) {
                        if (opinfo.op_type == operand_type::ss) {
                            op.mem.bits = 32;
                        } else if (opinfo.op_type == operand_type::sd) {
                            op.mem.bits = 64;
                        } else {
                            assert(opinfo.op_type == operand_type::ps || opinfo.op_type == operand_type::x);
                            op.mem.bits = 128;
                        }
                    } else {
                        assert(opinfo.addr_mode == addressing_mode::E || opinfo.addr_mode == addressing_mode::M);
                        if (opinfo.op_type == operand_type::mem) {
                            assert(!(ins.prefixes & prefix_flag_opsize));
                            op.mem.bits = 0;
                        } else if (opinfo.op_type == operand_type::b) {
                            assert(!(ins.prefixes & prefix_flag_opsize));
                            op.mem.bits = 8;
                        } else if (opinfo.op_type == operand_type::w) {
                            assert(!(ins.prefixes & prefix_flag_opsize));
                            op.mem.bits = 16;
                        } else if (opinfo.op_type == operand_type::d) {
                            assert(!(ins.prefixes & prefix_flag_opsize));
                            op.mem.bits = 32;
                        } else if (opinfo.op_type == operand_type::dq) {
                            assert(!(ins.prefixes & prefix_flag_opsize));
                            op.mem.bits = 64; // Really 128 bits, but objdump shows it as QWORD PTR
                        } else if (opinfo.op_type == operand_type::z) {
                            if (ins.prefixes & prefix_flag_opsize) {
                                op.mem.bits = 16;
                                used_prefixes |= prefix_flag_opsize;
                            } else {
                                op.mem.bits = 32;
                            }
                        } else if (opinfo.op_type == operand_type::y) {
                            if (rex & rex_w_mask) {
                                op.mem.bits = 64;
                                used_prefixes |= prefix_flag_rex | rex_w_mask;
                            } else {
                                op.mem.bits = 32;
                            }
                        } else {
                            assert(opinfo.op_type == operand_type::v);
                            op.mem.bits = rex & rex_w_mask ? 64 : 32;
                            used_prefixes |= prefix_flag_rex | rex_w_mask;
                            if (ins.prefixes & prefix_flag_opsize) {
                                assert(!(rex & rex_w_mask));
                                op.mem.bits = 16;
                                used_prefixes |= prefix_flag_opsize;
                            }
                        }
                    }
                }
                break;
            }
            case addressing_mode::G:
                if (ins.prefixes & prefix_flag_opsize) {
                    op.type = decoded_operand_type::reg16;
                    op.reg = modrm_reg(modrm);
                    if (rex & rex_r_mask) {
                        op.reg += 8;
                        used_prefixes |= prefix_flag_rex | rex_r_mask;
                    }
                    used_prefixes |= prefix_flag_opsize;
                } else {
                    op = do_reg(opinfo.op_type, modrm_reg(modrm) + (rex & rex_r_mask ? 8 : 0), rex);
                    used_prefixes |= prefix_flag_rex | rex_r_mask | rex_w_mask;
                }
                break;
            case addressing_mode::I:
                if (opinfo.op_type == operand_type::b || opinfo.op_type == operand_type::ub) {
                    do_imm8();
                    if (opinfo.op_type == operand_type::ub) {
                        op.imm.value &= 0xff;
                    }
                } else if (opinfo.op_type == operand_type::v) {
                    if (rex & rex_w_mask) {
                        do_imm64();
                        used_prefixes |= prefix_flag_rex | rex_w_mask;
                    } else {
                        if (ins.prefixes & prefix_flag_opsize) {
                            do_imm16();
                            used_prefixes |= prefix_flag_opsize;
                        } else {
                            do_imm32();
                        }
                    }
                } else if (opinfo.op_type == operand_type::w) {
                    do_imm16();
                } else if (opinfo.op_type == operand_type::z) {
                    if (ins.prefixes & prefix_flag_opsize) {
                        do_imm16();
                        used_prefixes |= prefix_flag_opsize;
                    } else {
                        do_imm32();
                    }
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
                op.type = decoded_operand_type::creg;
                op.reg = modrm_reg(modrm) + (rex & rex_r_mask ? 8 : 0);
                used_prefixes |= prefix_flag_rex | rex_r_mask;
                break;
            case addressing_mode::D:
                assert(!(rex&rex_r_mask));
                assert(opinfo.op_type == operand_type::d);
                op.type = decoded_operand_type::dreg;
                op.reg = modrm_reg(modrm);
                break;
            // The reg field of the ModR / M byte selects a segment register (for example, MOV(8C, 8E)).
            case addressing_mode::S:
                assert(!(rex&rex_r_mask));
                op.type = decoded_operand_type::sreg;
                op.reg = modrm_reg(modrm);
                break;
            case addressing_mode::R:
                {
                    // The R / M field of the ModR / M byte may refer only to a general register (for example, MOV(0F20 - 0F23)).
                    assert(opinfo.op_type == operand_type::d);
                    const uint8_t mod = modrm_mod(modrm);
                    assert(mod == 3);
                    op.type = decoded_operand_type::reg64;
                    op.reg  = modrm_rm(modrm) + (rex & rex_b_mask ? 8 : 0);
                    used_prefixes |= prefix_flag_rex | rex_b_mask;
                }
                break;
            case addressing_mode::U:
                assert(opinfo.op_type == operand_type::x);
                assert(modrm_mod(modrm) == 3);
                op.type = decoded_operand_type::xmmreg;
                op.reg = modrm_rm(modrm) + (rex & rex_r_mask ? 8 : 0);
                used_prefixes |= prefix_flag_rex | rex_r_mask;
                break;
            case addressing_mode::V:
                assert(opinfo.op_type == operand_type::ss || opinfo.op_type == operand_type::sd || opinfo.op_type == operand_type::ps || opinfo.op_type == operand_type::x || opinfo.op_type == operand_type::q || opinfo.op_type == operand_type::y);
                op.type = decoded_operand_type::xmmreg;
                op.reg = modrm_reg(modrm) + (rex & rex_r_mask ? 8 : 0);
                used_prefixes |= prefix_flag_rex | rex_r_mask;
                break;
            case addressing_mode::O:
                assert(opinfo.op_type == operand_type::b || opinfo.op_type == operand_type::v);
                //assert(rex & rex_w_mask);
                op.type = decoded_operand_type::absaddr;
                op.absaddr = get_u64();
                //used_prefixes |= prefix_flag_rex | rex_w_mask;
                break;
            default:
                assert(false);
        }
    }
    ins.unused_prefixes = ins.prefixes & ~used_prefixes;
    if (restore_rex_w_mask) {
        ins.unused_prefixes |= prefix_flag_rex | rex_w_mask;
    }
    return ins;
}

} } // namespace attos::disasm


using namespace attos;
using namespace attos::pe;
using namespace attos::disasm;

void disasm_section(uint64_t virtual_address, const uint8_t* code, uint64_t virtual_end)
{
    while (virtual_address < virtual_end) {
        auto ins = do_disasm(code);

        if (virtual_address < (1ULL<<48)) dbgout() << "  "; // Stupid hack to match objdump output
        dbgout() << as_hex(virtual_address).width(0) << ":\t";
        for (int i = 0; i < 12; ++i) {
            if (i < ins.len) {
                dbgout() << as_hex(ins.start[i]) << ' ';
            } else {
                dbgout() << "   ";
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
        uint32_t used_prefixes = 0;
        if (ins.prefixes & prefix_flag_lock) {
            iwidth -= 5;
            dbgout() << "lock ";
            used_prefixes |= prefix_flag_lock;
        }
        if (ins.prefixes & prefix_flag_rep) {
            iwidth -= 4;
            dbgout() << "rep ";
            used_prefixes |= prefix_flag_rep;
            used_prefixes |= prefix_flag_opsize; // HACK: ignore db 66 for string instructions
        }
        if ((ins.unused_prefixes & prefix_flag_rex) != 0 && info->operands[0].op_type != operand_type::none) { // HACK: Don't show rex prefix for single byte instructions (pushfq/cdqe)
            const char* const rex_names[16] = {
                "rex", "rex.B", "rex.X", "rex.XB", "rex.R", "rex.RB", "rex.RX", "rex.RXB",
                "rex.W", "rex.WB", "rex.WX", "rex.WXB", "rex.WR", "rex.WRB", "rex.WRX", "rex.WRXB",
            };
            const char* const rn = rex_names[ins.unused_prefixes & 0xf];
            dbgout() << rn << " ";
            iwidth -= static_cast<int>(string_length(rn)) + 1;
        }

        if (!instruction_name) {
            dbgout() << "Unknown instruction\n";
            //assert(false);
            return;
        }

        dbgout() << format_str(instruction_name).width(iwidth > 0 ? iwidth : 0);

        bool show_comment_addr = false;
        uint64_t comment_addr = 0;
        int operation_size_bits = 0;

        for (int i = 0; i < max_operands && info->operands[i].op_type != operand_type::none; ++i) {
            dbgout() << (i ? "," : " ");
            const auto& op = ins.operands[i];
            switch (op.type) {
                case decoded_operand_type::reg8:
                    assert(op.reg<sizeof(reg8)/sizeof(*reg8));
                    dbgout() << reg8[op.reg];
                    operation_size_bits = 8;
                    break;
                case decoded_operand_type::reg8_rex:
                    assert(op.reg<sizeof(reg8_rex)/sizeof(*reg8_rex));
                    dbgout() << reg8_rex[op.reg];
                    operation_size_bits = 8;
                    break;
                case decoded_operand_type::reg16:
                    assert(op.reg<sizeof(reg16)/sizeof(*reg16));
                    dbgout() << reg16[op.reg];
                    operation_size_bits = 16;
                    break;
                case decoded_operand_type::reg32:
                    assert(op.reg<sizeof(reg32)/sizeof(*reg32));
                    dbgout() << reg32[op.reg];
                    operation_size_bits = 32;
                    break;
                case decoded_operand_type::reg64:
                    assert(op.reg<sizeof(reg64)/sizeof(*reg64));
                    dbgout() << reg64[op.reg];
                    operation_size_bits = 64;
                    break;
                case decoded_operand_type::xmmreg:
                    assert(op.reg<sizeof(xmmreg)/sizeof(*xmmreg));
                    dbgout() << xmmreg[op.reg];
                    operation_size_bits = 128;
                    break;
                case decoded_operand_type::sreg:
                    assert(op.reg<sizeof(sreg)/sizeof(*sreg));
                    dbgout() << sreg[op.reg];
                    operation_size_bits = 16;
                    break;
                case decoded_operand_type::creg:
                    assert(op.reg<sizeof(creg)/sizeof(*creg));
                    dbgout() << creg[op.reg];
                    operation_size_bits = 64;
                    break;
                case decoded_operand_type::dreg:
                    assert(op.reg<sizeof(dreg)/sizeof(*dreg));
                    dbgout() << dreg[op.reg];
                    operation_size_bits = 64;
                    break;
                case decoded_operand_type::disp8:
                case decoded_operand_type::disp32:
                    dbgout() << "0x" << as_hex(virtual_address + ins.len + op.disp).width(0);
                    break;
                case decoded_operand_type::immediate:
                    if (op.imm.bits == 1) {
                        dbgout() << "1";
                    } else {
                        const auto bits = operation_size_bits  && operation_size_bits < 128 ? operation_size_bits : op.imm.bits;
                        assert(bits);
                        const auto mask = bits == 64 ? ~0ULL : (1ULL << bits) - 1;
                        dbgout() << "0x" << as_hex(op.imm.value & mask).width(0);
                        //dbgout() << " immbits=" << op.imm.bits << "value=" << op.imm.value << "bits=" << bits << "mask = " << as_hex(mask);
                    }
                    break;
                case decoded_operand_type::mem:
                    if (op.mem.bits) {
                        if (op.mem.bits == 8) {
                            dbgout() << "BYTE";
                        } else if (op.mem.bits == 16) {
                            dbgout() << "WORD";
                        } else if (op.mem.bits == 32) {
                            dbgout() << "DWORD";
                        } else if (op.mem.bits == 64) {
                            dbgout() << "QWORD";
                        } else {
                            assert(op.mem.bits == 128);
                            dbgout() << "XMMWORD";
                        }
                        dbgout() << " PTR ";
                        operation_size_bits = op.mem.bits;
                    }
                    if (op.mem.reg == reg_rip) {
#if 0
                        dbgout() << "[" << "0x" << as_hex(virtual_address + ins.len + op.mem.disp).width(0) << "]";
#else
                        dbgout() << "[rip+" << "0x" << as_hex(op.mem.disp).width(0) << "]";
                        comment_addr = virtual_address + ins.len + op.mem.disp;
                        show_comment_addr = true;
#endif
                    } else {
                        bool brackets = true;

                        if (ins.prefixes & prefix_flag_cs) {
                            dbgout() << "cs:";
                            brackets = false;
                            used_prefixes |= prefix_flag_cs;
                        }

                        if (ins.prefixes & prefix_flag_gs) {
                            dbgout() << "gs:";
                            brackets = false;
                            used_prefixes |= prefix_flag_gs;
                        }

                        if (brackets) dbgout() << "[";
                        if (op.mem.reg != reg_invalid) {
                            dbgout() << reg64[op.mem.reg];
                            if (op.mem.reg2scale != 0) {
                                dbgout() << "+";
                            }
                        }
                        if (op.mem.reg2scale != 0) {
                            dbgout() << reg64[op.mem.reg2];
                            if (op.mem.reg2scale > 1) {
                                dbgout() << "*" << op.mem.reg2scale;
                            }
                        }
                        if (op.mem.dispbits) {
                            if (op.mem.disp < 0) {
                                dbgout() << "-0x" << as_hex(-op.mem.disp).width(0);
                            } else {
                                if (brackets) dbgout() << "+";
                                dbgout() << "0x" << as_hex(op.mem.disp).width(0);
                            }
                        }
                        if (brackets) dbgout() << "]";
                    }
                    break;
                case decoded_operand_type::absaddr:
                    dbgout() << "ds:0x" << as_hex(op.absaddr).width(0);
                    break;
                default:
                    assert(false);
            }
        }
        if (show_comment_addr) {
            dbgout() << "    # 0x" << as_hex(comment_addr).width(0);
        }
        dbgout() << "\n";

        const auto ignore_mask = 0x4F | used_prefixes; // Ignore unused reg prefixes
        if (ins.unused_prefixes & ~ignore_mask) {
            dbgout() << "Unused prefixes:" << as_hex(ins.unused_prefixes & ~ignore_mask) << "\n";
            return;
        }

        if ((virtual_address&0xffff)==0x69C1) {
            //system("pause");
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
        os_.flush();
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
    fclose(fp);
    return v;
}

int main(int argc, char* argv[])
{
    attos_stream_wrapper asw{std::cout};
#if 1
    const bool use_hack = argc > 2 && argv[2][0] == 'H';
    const auto f = read_file(argv[1]);
    const auto& image = *reinterpret_cast<const IMAGE_DOS_HEADER*>(f.data());
    for (const auto section : image.nt_headers().sections()) {
        if ((section.Characteristics & IMAGE_SCN_CNT_CODE) == 0) {
            continue;
        }
        auto offset = use_hack ? image.nt_headers().OptionalHeader.AddressOfEntryPoint - section.VirtualAddress : 0;
        if (offset < 0 || offset >= section.SizeOfRawData) offset = 0;
        uint64_t virtual_address = image.nt_headers().OptionalHeader.ImageBase + section.VirtualAddress + offset;
        disasm_section(virtual_address, &f[section.PointerToRawData + offset], virtual_address + section.Misc.VirtualSize);
    }
#else
    disasm_section(reinterpret_cast<uint64_t>(_ReturnAddress()), reinterpret_cast<const uint8_t*>(_ReturnAddress()), 20);
    dbgout()<<"\n";
    const uint8_t* code = &mainCRTStartup[0];
    if (code[0] == 0xE9) code += 5 + *(uint32_t*)&code[1];
    disasm_section(reinterpret_cast<uint64_t>(code), code, 10000);
#endif
}

#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <stdlib.h>
#include <stdint.h>
#include <intrin.h>
#include <assert.h>
#include <algorithm>

#include <attos/pe.h>
#include <attos/cpu.h>
#include <attos/out_stream.h>

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
C The reg field of the ModR / M byte selects a control register (for example, MOV(0F20, 0F22)).
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
can be applied(for example, MOV(A0–A3)).
P The reg field of the ModR / M byte selects a packed quadword MMX technology register.
Q A ModR / M byte follows the opcode and specifies the operand.The operand is either an MMX technology
register or a memory address.If it is a memory address, the address is computed from a segment register
and any of the following values : a base register, an index register, a scaling factor, and a displacement.
R The R / M field of the ModR / M byte may refer only to a general register (for example, MOV(0F20 - 0F23)).
S The reg field of the ModR / M byte selects a segment register (for example, MOV(8C, 8E)).
U The R / M field of the ModR / M byte selects a 128 - bit XMM register or a 256 - bit YMM register, determined by
operand type.
V The reg field of the ModR / M byte selects a 128 - bit XMM register or a 256 - bit YMM register, determined by
operand type.
W A ModR / M byte follows the opcode and specifies the operand.The operand is either a 128 - bit XMM register,
a 256 - bit YMM register (determined by operand type), or a memory address.If it is a memory address, the
address is computed from a segment register and any of the following values : a base register, an index
register, a scaling factor, and a displacement.
X Memory addressed by the DS : rSI register pair(for example, MOVS, CMPS, OUTS, or LODS).
Y Memory addressed by the ES : rDI register pair(for example, MOVS, CMPS, INS, STOS, or SCAS).

A.2.2 Codes for Operand Type
a Two one - word operands in memory or two double - word operands in memory, depending on operand - size attribute(used only by the BOUND instruction).
c Byte or word, depending on operand - size attribute.
d Doubleword, regardless of operand - size attribute.
dq Double - quadword, regardless of operand - size attribute.
p 32 - bit, 48 - bit, or 80 - bit pointer, depending on operand - size attribute.
pd 128 - bit or 256 - bit packed double - precision floating - point data.
pi Quadword MMX technology register (for example: mm0).
ps 128 - bit or 256 - bit packed single - precision floating - point data.
qq Quad - Quadword(256 - bits), regardless of operand - size attribute.
s 6 - byte or 10 - byte pseudo - descriptor.
sd Scalar element of a 128 - bit double - precision floating data.
ss Scalar element of a 128 - bit single - precision floating data.
si Doubleword integer register (for example: eax).
w Word, regardless of operand - size attribute.
x dq or qq based on the operand - size attribute.
y Doubleword or quadword(in 64 - bit mode), depending on operand - size attribute.
#endif


enum class addressing_mode : uint8_t {
    low_instruction_bits,
    rax,
    E, // A ModR / M byte follows the opcode and specifies the operand.The operand is either a general - purpose register or a memory address.If it is a memory address, the address is computed from a segment register and any of the following values : a base register, an index register, a scaling factor, a displacement.
    G, // The reg field of the ModR / M byte selects a general register (for example, AX(000)).
    I, // Immediate data : the operand value is encoded in subsequent bytes of the instruction.
    J, // The instruction contains a relative offset to be added to the instruction pointer register (for example, JMP(0E9), LOOP).
    M, // The ModR / M byte may refer only to memory(for example, BOUND, LES, LDS, LSS, LFS, LGS, CMPXCHG8B).
};

enum class operand_type : uint8_t {
    none,
    mem,
    b, // Byte, regardless of operand - size attribute.
    v, // Word, doubleword or quadword(in 64 - bit mode), depending on operand - size attribute.
    q, // Quadword, regardless of operand - size attribute.
    z, // z Word for 16 - bit operand - size or doubleword for 32 or 64 - bit operand - size.
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
struct instruction_info {
    instruction_info_type type;
    union {
        const char*             name;
        const char* const*      group_names;
        const instruction_info *group;
    };
    instruction_operand_info operands[max_operands];

    constexpr instruction_info() = default;
    constexpr instruction_info(const char* name, instruction_operand_info op0=instruction_operand_info{}, instruction_operand_info op1=instruction_operand_info{})
        : type(instruction_info_type::normal), name(name), operands{op0, op1} {
    }

    constexpr instruction_info(const char* const* group_names, instruction_operand_info op0=instruction_operand_info{}, instruction_operand_info op1=instruction_operand_info{})
        : type(instruction_info_type::group_names), group_names(group_names), operands{op0, op1} {
    }

    constexpr instruction_info(const instruction_info* group, instruction_operand_info op0=instruction_operand_info{}, instruction_operand_info op1=instruction_operand_info{})
        : type(instruction_info_type::group), group(group), operands{op0, op1} {
    }
};

enum class decoded_operand_type {
    reg8,
    reg8_rex,
    reg32,
    reg64,
    disp8,
    disp32,
    imm8,
    imm32,
    mem,
};

struct mem_op {
    uint8_t reg;
    //uint8_t reg2;
    //uint8_t reg2scale;
    int     dispbits;
    int32_t disp;
};

struct decoded_operand {
    decoded_operand_type type;
    union {
        uint8_t  reg;
        int32_t  disp;
        uint32_t imm;
        mem_op   mem;
    };
};

const uint8_t rex_b_mask = 0x1; // Extension of r/m field, base field, or opcode reg field
const uint8_t rex_x_mask = 0x2; // Extension of extension of SIB index field
const uint8_t rex_r_mask = 0x4; // Extension of ModR/M reg field
const uint8_t rex_w_mask = 0x8; // 64 Bit Operand Size

decoded_operand do_reg(operand_type type, uint8_t reg, bool rex_active)
{
    decoded_operand op;
    op.reg = reg;
    switch (type) {
        case operand_type::b:
            //op.type = rex == 0x40? decoded_operand_type::reg8_rex : decoded_operand_type::reg8;
            op.type = rex_active ? decoded_operand_type::reg8_rex : decoded_operand_type::reg8;
            break;
        case operand_type::v:
            //op.type = (rex & rex_w_mask) ? decoded_operand_type::reg64 : decoded_operand_type::reg32;
            op.type = rex_active ? decoded_operand_type::reg64 : decoded_operand_type::reg32;
            break;
        default:
            assert(false);
    }
    return op;
}

struct decoded_instruction {
    const uint8_t*          start;
    int                     len;
    const instruction_info* info;
    decoded_operand         operands[max_operands];
    int                     instruction_offset;
    int                     modrm_offset;
};

instruction_info instructions[256];

bool has_modrm(const instruction_info& ii)
{
    for (int i = 0; i < max_operands && ii.operands[i].op_type != operand_type::none; ++i) {
        switch (ii.operands[i].addr_mode) {
            case addressing_mode::E:
            case addressing_mode::G:
            case addressing_mode::M:
                return true;
            case addressing_mode::low_instruction_bits:
            case addressing_mode::rax:
            case addressing_mode::I:
            case addressing_mode::J:
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

const char* const reg8_rex[32]  = {
    "al",  "cl",  "dl", "bl", "spl", "bpl", "sil", "dil"
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
};
const char* const reg8[8] = {
    "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
};

const char* const reg32[16] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi", "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d" };
const char* const reg64[16] = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };

namespace operand_info_helpers {
#define II(addr_mode, type) instruction_operand_info{addressing_mode::addr_mode, operand_type::type}
constexpr auto Eb = II(E, b);
constexpr auto Ev = II(E, v);
constexpr auto Gb = II(G, b);
constexpr auto Gv = II(G, v);
constexpr auto Ib = II(I, b);
constexpr auto Iv = II(I, v);
constexpr auto Iz = II(I, z);
constexpr auto M  = II(M, mem);
constexpr auto Jb = II(J, b);
constexpr auto Jz = II(J, z);
constexpr auto R8 = II(low_instruction_bits, b);
constexpr auto R64 = II(low_instruction_bits, q);
constexpr auto rAL = II(rax, b);
#undef II
};

decoded_instruction do_disasm(const uint8_t* code)
{
    static bool init = false;
    if (!init) {
        using namespace operand_info_helpers;
        instructions[0x03] = instruction_info{"add",  Gv, Ev};     // ADD r16/32/64, r/m16/32/64
        instructions[0x1B] = instruction_info{"sbb",  Gv, Ev};     // SBB r16/32/64, r/m16/32/64
        instructions[0x24] = instruction_info{"and",  rAL, Ib};    // AND AL, imm8

        instructions[0x33] = instruction_info{"xor",  Gv, Ev};     // XOR r16/32/64, r/m16/32/64

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

        instructions[0x74] = instruction_info{"je",   Jb};         // JE rel8
        instructions[0x75] = instruction_info{"jne",  Jb};         // JNE rel8

        static const char* const group1[8] = { "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp" };

        instructions[0x80] = instruction_info{group1, Eb, Ib};     // Immediate Grp 1
        instructions[0x81] = instruction_info{group1, Ev, Iz};     // Immediate Grp 1
        //instructions[0x82] = instruction_info{group1, Eb, Ib};     // Immediate Grp 1
        instructions[0x83] = instruction_info{group1, Ev, Ib};     // Immediate Grp 1
        instructions[0x84] = instruction_info{"test", Eb, Gb};     // TEST r/m8, r8
        instructions[0x85] = instruction_info{"test", Ev, Gv};     // TEST r/m16/32/64, r16/32/64

        instructions[0x89] = instruction_info{"mov",  Ev, Gv};     // MOV r/m16/32/64, r16/32/64
        instructions[0x8b] = instruction_info{"mov",  Gv, Ev};     // MOV r16/32/64, r/m16/32/64
        instructions[0x8d] = instruction_info{"lea",  Gv, M};      // LEA r16/32/64

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

        instructions[0xc3] = instruction_info{"ret"};              // RET
        instructions[0xcc] = instruction_info{"int3"};             // INT3
        instructions[0xe8] = instruction_info{"call", Jz};         // CALL rel16/rel32
        instructions[0xe9] = instruction_info{"jmp",  Jz};         // JMP rel16/rel32
        instructions[0xeb] = instruction_info{"jmp",  Jb};         // JMP rel8
        static const instruction_info group3[8] = {
            { "test", Ib, Iz  },
            {},
            { "not",  Eb      },
            { "neg",  Eb      },
            { "mul",  rAL, Eb },
            { "imul", rAL, Eb },
            { "div",  rAL, Eb },
            { "idiv", rAL, Eb },
        };

        instructions[0xf6] = instruction_info{group3, Eb};         // Unary Grp 3
        instructions[0xf7] = instruction_info{group3, Ev};         // Unary Grp 3
        init = true;
    }

    decoded_instruction ins;
    ins.start = code;
    ins.len   = 0;

    auto peek_u8 = [&] { return ins.start[ins.len]; };
    auto get_u8  = [&] { return ins.start[ins.len++]; };
    auto get_u32 = [&] { auto disp = *(uint32_t*)&ins.start[ins.len]; ins.len += 4; return disp; };

    uint8_t rex = 0;
    if ((peek_u8() & 0xf0) == 0x40) {
        rex = ins.start[ins.len++];
    }

    ins.instruction_offset = ins.len;
    const uint8_t instruction_byte = get_u8();

    ins.info  = &instructions[instruction_byte];

    uint8_t modrm = 0;
    if (has_modrm(*ins.info)) {
        ins.modrm_offset = ins.len;
        modrm = get_u8();
    } else {
        ins.modrm_offset = -1;
    }


    for (int i = 0; i < max_operands && ins.info->operands[i].op_type != operand_type::none; ++i) {
        const auto& opinfo = ins.info->operands[i];
        auto& op = ins.operands[i];
        switch (opinfo.addr_mode) {
            case addressing_mode::low_instruction_bits:
            {
                assert((rex & 0xe) == 0);
                op.reg = instruction_byte & 7;
                if (rex & rex_b_mask) {
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
                assert(opinfo.op_type == operand_type::b);
                op.type = decoded_operand_type::reg8;
                op.reg  = 0;
            break;
            case addressing_mode::E:
            case addressing_mode::M:
            {
                const uint8_t mod = modrm_mod(modrm);
                const uint8_t rm  = modrm_rm(modrm);
                if (mod == 3) {
                    op = do_reg(opinfo.op_type, rm + (rex & rex_b_mask ? 8 : 0) , rex & rex_w_mask ? true : false);
                } else {
                    op.type    = decoded_operand_type::mem;
                    if (rm != 4) {
                        op.mem.reg = rm;
                    } else {
                        // SIB
                        const uint8_t sib = get_u8();
                        const uint8_t reg2  = (sib >> 3) & 7;
                        const uint8_t scale = 1<<((sib>>6)&3);
                        assert(reg2 == 4); // 4 means no scaled register
                        op.mem.reg = sib & 7;
                    }
                    if (mod == 0) {
                        op.mem.dispbits = 0;
                        op.mem.disp = 0;
                    } else if (mod == 1) {
                        op.mem.dispbits = 8;
                        op.mem.disp = static_cast<int8_t>(get_u8());
                    } else if (mod == 2) {
                        op.mem.dispbits = 32;
                        op.mem.disp = static_cast<int32_t>(get_u32());
                    }
                }
                break;
            }
            case addressing_mode::G:
                op = do_reg(opinfo.op_type, modrm_reg(modrm) + (rex & rex_r_mask ? 8 : 0), rex & rex_w_mask ? true : false);
                break;
            case addressing_mode::I:
                if (opinfo.op_type == operand_type::b) {
                    op.type = decoded_operand_type::imm8;
                    op.imm  = get_u8();
                } else if (opinfo.op_type == operand_type::v) {
                    // Word, doubleword or quadword(in 64 - bit mode), depending on operand - size attribute.
                    assert(!rex);
                    op.type = decoded_operand_type::imm32;
                    op.imm  = get_u32();
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
            default:
                assert(false);
        }
    }
    return ins;
}

} } // namespace attos::disasm


using namespace attos;
using namespace attos::pe;
using namespace attos::disasm;

extern "C" IMAGE_DOS_HEADER __ImageBase;

void disasm_section(uint64_t virtual_address, const uint8_t* code, int maxinst)
{
    for (int count=0;count<maxinst;count++) {
        auto ins = do_disasm(code);

        dbgout() << as_hex(virtual_address);
        for (int i = 0; i < ins.len; ++i) {
            dbgout() << ' ' << as_hex(ins.start[i]);
        }
        for (int i = ins.len; i < 8; ++i) {
            dbgout() << "   ";
        }
        if (!ins.info->name) {
            dbgout() << "Unknown instruction\n";
            return;
        }

        const char* instruction_name;
        if (ins.info->type == instruction_info_type::normal) {
            instruction_name = ins.info->name;
        } else {
            assert(ins.modrm_offset >= 0 && ins.modrm_offset <= ins.len);
            const auto reg = modrm_reg(ins.start[ins.modrm_offset]);
            if (ins.info->type == instruction_info_type::group_names) {
                instruction_name = ins.info->group_names[reg];
            } else {
                assert(ins.info->type == instruction_info_type::group);
                ins.info = &ins.info->group[reg];
                instruction_name = ins.info->name;
            }
        }
        dbgout() << format_str(instruction_name).width(8);

        for (int i = 0; i < max_operands && ins.info->operands[i].op_type != operand_type::none; ++i) {
            dbgout() << (i ? ", " : " ");
            const auto& op = ins.operands[i];
            switch (op.type) {
                case decoded_operand_type::reg8:
                    dbgout() << reg8[op.reg];
                    break;
                case decoded_operand_type::reg8_rex:
                    dbgout() << reg8_rex[op.reg];
                    break;
                case decoded_operand_type::reg32:
                    dbgout() << reg32[op.reg];
                    break;
                case decoded_operand_type::reg64:
                    dbgout() << reg64[op.reg];
                    break;
                case decoded_operand_type::disp8:
                case decoded_operand_type::disp32:
                    dbgout() << as_hex(virtual_address + ins.len + op.disp);
                    break;
                case decoded_operand_type::imm8:
                case decoded_operand_type::imm32:
                    dbgout() << as_hex(op.imm).width(0);
                    break;
                case decoded_operand_type::mem:
                    dbgout() << "[" << reg64[op.mem.reg];
                    if (op.mem.dispbits) {
                        if (op.mem.disp < 0) {
                            dbgout() << "-" << as_hex(-op.mem.disp).width(0);
                        } else {
                            dbgout() << "+" << as_hex(op.mem.disp).width(0);
                        }
                    }
                    dbgout() << "]";
                    break;
                default:
                    assert(false);
            }
        }
        dbgout() << "\n";

        code            += ins.len;
        virtual_address += ins.len;
    }
}

extern "C" const uint8_t mainCRTStartup[];

int main()
{
    attos_stream_wrapper asw{std::cout};
#if 0
    for (const auto section : __ImageBase.nt_headers().sections()) {
        if ((section.Characteristics & IMAGE_SCN_CNT_CODE) == 0) {
            continue;
        }
        uint64_t virtual_address = __ImageBase.nt_headers().OptionalHeader.ImageBase + section.VirtualAddress;
        disasm_section(virtual_address, (const uint8_t*)virtual_address);
    }
#else
    disasm_section(reinterpret_cast<uint64_t>(_ReturnAddress()), reinterpret_cast<const uint8_t*>(_ReturnAddress()), 20);
    dbgout()<<"\n";
    const uint8_t* code = &mainCRTStartup[0];
    if (code[0] == 0xE9) code += 5 + *(uint32_t*)&code[1];
    disasm_section(reinterpret_cast<uint64_t>(code), code, 52);
#endif
}

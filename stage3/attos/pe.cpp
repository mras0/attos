#include "pe.h"
#include "cpu.h"

namespace attos { namespace pe {

const char* const unwind_reg_names[16] = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8 ", "r9 ", "r10", "r11", "r12", "r13", "r14", "r15" };

const RUNTIME_FUNCTION* runtime_function_info(array_view<RUNTIME_FUNCTION> rfs, uint32_t function_rva)
{
    //printf("Searching for 0x%x\n", function_rva);
    // TODO: Use the fact that the list is sorted to perform a binary search
    auto it = std::find_if(rfs.begin(), rfs.end(), [function_rva](const RUNTIME_FUNCTION& rf) { return rf.BeginAddress <= function_rva && function_rva <= rf.EndAddress; });
    if (it == rfs.end()) return nullptr;
    return it;
}

const uint64_t* unwind_once(const IMAGE_DOS_HEADER& image, uint64_t rip, const uint64_t* rsp)
{
    const auto function_rva = static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(rip) - reinterpret_cast<const uint8_t*>(&image));
    const auto rf = runtime_function_info(image.data_directory<RUNTIME_FUNCTION>(), function_rva);
    if (rf == nullptr) {
        //printf("\tFunction at RVA %x (RIP = %llx) is a leaf function\n", function_rva, rip);
        // If no function table entry is found, then it is in a leaf function, and RSP will directly address the return pointer.
        // The return pointer at [RSP] is stored in the updated context, the simulated RSP is incremented by 8
        return rsp + 1;
    }
    const auto& ui = detail::rva_cast<UNWIND_INFO>(image, rf->UnwindInfoAddress);

    const auto offset_in_function = function_rva - rf->BeginAddress;
    REQUIRE(ui.Version == UNWIND_INFO_VERSION);
    // TODO: We don't handle UNWIND_INFO_FLAG_EHANDLER / UNWIND_INFO_FLAG_UHANDLER
    REQUIRE((ui.Flags & UNWIND_INFO_FLAG_CHAININFO) == 0);
    REQUIRE(ui.FrameRegister == 0);
    REQUIRE(ui.FrameOffset == 0);

    if (offset_in_function <= ui.SizeOfProlog) {
        // We don't handle unwinds inside the prolog
        REQUIRE(false);
    }
    // TODO: detect if we're inside the epilog...

    for (int i = 0; i < ui.CountOfCodes; ++i) {
        const auto& uc = ui.unwind_codes()[i];
        switch (uc.UnwindOp) {
            case UWOP_PUSH_NONVOL:
                // Push a nonvolatile integer register, decrementing RSP by 8
                //printf("\tpop %s\n", unwind_reg_names[uc.OpInfo]);
                rsp += 1; // Pop
                break;
            case UWOP_ALLOC_LARGE:
            {
                REQUIRE(uc.OpInfo == 0); // No support for >512K-8 byte stacks, they use 3 nodes
                const auto qword_size = *reinterpret_cast<const uint16_t*>(&ui.unwind_codes()[++i]); // Consume unwind code
                //printf("\tadd rsp, 0x%X\n", qword_size * 8);
                rsp += qword_size;
                break;
            }
            case UWOP_ALLOC_SMALL:
            {
                const auto qwords = (uc.OpInfo+1);
                //printf("\tadd rsp, 0x%X\n", qwords*8);
                rsp += qwords;
                break;
            }
            case UWOP_SET_FPREG:
                //printf("Unhandled: UWOP_SET_FPREG %d\n", uc.OpInfo);
                REQUIRE(false);
                break;
            case UWOP_SAVE_NONVOL:
            {
                const auto offset = 8 * *reinterpret_cast<const uint16_t*>(&ui.unwind_codes()[++i]); // Consume unwind code
                //printf("\tmov %s, [rsp+0x%X]\n", unwind_reg_names[uc.OpInfo], offset);
                break;
            }
            case UWOP_SAVE_NONVOL_FAR:
            case UWOP_SAVE_XMM128:
            case UWOP_SAVE_XMM128_FAR:
            case UWOP_PUSH_MACHFRAME:
                //printf("Unhandled unwind OP %d\n", uc.UnwindOp);
                REQUIRE(false);
                break;
        }
    }
    return rsp + 1; // Finally 'pop' return address
}


} } // namespace attos::pe

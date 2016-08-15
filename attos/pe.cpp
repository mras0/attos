#include "pe.h"
#include <attos/cpu.h>
#include <attos/out_stream.h>

#define SHOW_OPS_VERBOSE 0

namespace attos { namespace pe {

uint32_t file_size_from_header(const IMAGE_DOS_HEADER& image) {
    REQUIRE(image.e_magic == IMAGE_DOS_SIGNATURE);
    uint32_t size = 0;
    for (const auto& s : image.nt_headers().sections()) {
        if (s.SizeOfRawData) {
            size = std::max(size, s.PointerToRawData + s.SizeOfRawData);
        }
    }
    return size;
}

//
// Unwind (private)
//

namespace {
const char* const unwind_reg_names[16] = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8 ", "r9 ", "r10", "r11", "r12", "r13", "r14", "r15" };
const char* const unwind_xreg_names[16] = { "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15" };

const RUNTIME_FUNCTION* runtime_function_info(array_view<RUNTIME_FUNCTION> rfs, uint32_t function_rva) {
#if SHOW_OPS_VERBOSE >= 2
    dbgout() << "Searching for " << as_hex(function_rva) << "\n";
#endif

    // TODO: Use the fact that the list is sorted to perform a binary search
    auto it = std::find_if(rfs.begin(), rfs.end(), [function_rva](const RUNTIME_FUNCTION& rf) {
            //dbgout() << " checking " << as_hex(rf.BeginAddress) << " " << as_hex(rf.EndAddress) << "\n";
            return rf.BeginAddress <= function_rva && function_rva <= rf.EndAddress;
    });
    if (it == rfs.end()) return nullptr;
    return it;
}

struct unwind_context {
    uint64_t rsp;
    uint64_t rip;

    constexpr const uint64_t* stack_ptr() const {
        return reinterpret_cast<const uint64_t*>(rsp);
    }

    unwind_context ret() const {
        return { rsp + 8, stack_ptr()[0] };
    }

    unwind_context iret() const {
        return { stack_ptr()[3], stack_ptr()[0] };
    }
};

// Unwind the stack once. Returns `rsp' ready to pop the next return address
unwind_context unwind_once(const unwind_context& context, const IMAGE_DOS_HEADER& image) {
    const auto function_rva = static_cast<uint32_t>(context.rip - reinterpret_cast<uint64_t>(&image)); // TODO: REQUIRE(in_base(...))
    const auto rf = runtime_function_info(image.data_directory<RUNTIME_FUNCTION>(), function_rva);
    if (rf == nullptr) {
#if SHOW_OPS_VERBOSE >= 1
        dbgout() << "    Assuming function at RVA " << as_hex(function_rva) << " (RIP = " << as_hex(context.rip) << ") is a leaf function\n";
#endif
        // If no function table entry is found, then it is in a leaf function, and RSP will directly address the return pointer.
        return context.ret();
    }
    const auto& ui = detail::rva_cast<UNWIND_INFO>(image, rf->UnwindInfoAddress);

    const auto offset_in_function = function_rva - rf->BeginAddress;
    REQUIRE(ui.Version == UNWIND_INFO_VERSION);
    // TODO: We don't handle UNWIND_INFO_FLAG_EHANDLER / UNWIND_INFO_FLAG_UHANDLER
    REQUIRE((ui.Flags & UNWIND_INFO_FLAG_CHAININFO) == 0);
    if (ui.FrameRegister != 0) {
#if SHOW_OPS_VERBOSE >= 1
        dbgout() << "    Unhandled: Frame register " << unwind_reg_names[ui.FrameRegister] << " = rsp + 0x" << as_hex(16*ui.FrameOffset).width(0) << "\n";
#endif
    } else {
        REQUIRE(ui.FrameOffset == 0);
    }

    // TODO: detect if we're inside the epilog...

    auto new_context = context;
    for (int i = 0; i < ui.CountOfCodes; ++i) {
        const auto& uc = ui.unwind_codes()[i];
        bool ignore = false;
        if (uc.CodeOffset > offset_in_function) {
            // Skip unwind codes that didn't have a chance to execute
            // Note: This fails if the fault happens on an unwinding instruction, but in that case the stack is already messed up
#if SHOW_OPS_VERBOSE >= 1
            dbgout() << "Ignoring uc.CodeOffset = " << as_hex(uc.CodeOffset) << " (offset_in_function " << as_hex(offset_in_function).width(2) << ")\n";
#endif
            ignore = true;
        }
        switch (uc.UnwindOp) {
            case UWOP_PUSH_NONVOL:
                // Push a nonvolatile integer register, decrementing RSP by 8
#if SHOW_OPS_VERBOSE >= 1
                dbgout() << "    pop " << unwind_reg_names[uc.OpInfo] << "\n";
#endif
                if (!ignore) {
                    new_context.rsp += 8; // Pop
                }
                break;
            case UWOP_ALLOC_LARGE:
            {
                REQUIRE(uc.OpInfo == 0); // No support for >512K-8 byte stacks, they use 3 nodes
                const auto bytes = 8 * *reinterpret_cast<const uint16_t*>(&ui.unwind_codes()[++i]); // Consume unwind code
#if SHOW_OPS_VERBOSE >= 1
                dbgout() << "    add rsp, 0x" << as_hex(bytes).width(0) << "\n";
#endif
                if (!ignore) {
                    new_context.rsp += bytes;
                }
                break;
            }
            case UWOP_ALLOC_SMALL:
            {
                const auto bytes = 8*(uc.OpInfo+1);
#if SHOW_OPS_VERBOSE >= 1
                dbgout() << "    add rsp, 0x" << as_hex(bytes).width(0) << "\n";
#endif
                if (!ignore) {
                    new_context.rsp += bytes;
                }
                break;
            }
            case UWOP_SET_FPREG:
#if SHOW_OPS_VERBOSE >= 1
                dbgout() << "Unhandled: frame pointer = rsp + 0x" << as_hex(16*uc.OpInfo).width(0) << "\n";
#endif
                break;
            case UWOP_SAVE_NONVOL:
            {
                const auto offset = 8 * *reinterpret_cast<const uint16_t*>(&ui.unwind_codes()[++i]); // Consume unwind code
#if SHOW_OPS_VERBOSE >= 1
                dbgout() << "    mov " << unwind_reg_names[uc.OpInfo] << ", [rsp+0x" << as_hex(offset).width(0) << "]\n";
#endif
                break;
            }
            case UWOP_SAVE_XMM128:
            {
                const auto offset = 16 * *reinterpret_cast<const uint16_t*>(&ui.unwind_codes()[++i]); // Consume unwind code
#if SHOW_OPS_VERBOSE >= 1
                dbgout() << "    movaps " << unwind_xreg_names[uc.OpInfo] << ", [rsp+0x" << as_hex(offset).width(0) << "]\n";
#endif
                break;
            }
            case UWOP_SAVE_NONVOL_FAR:
            case UWOP_SAVE_XMM128_FAR:
                dbgout() << "Unhandled unwind OP 0x" << as_hex(uc.UnwindOp) << "\n";
                REQUIRE(false);
                break;
            case UWOP_PUSH_MACHFRAME:
            {
                REQUIRE(uc.OpInfo == 0);
                // RIP, CS, EFLAGS, Old RSP, SS
#if SHOW_OPS_VERBOSE >= 1
                dbgout() << "    iretq ; !!!!\n";
#endif
                if (!ignore) {
                    return new_context.iret();
                }
            }
        }
    }
    return new_context.ret();
}

// The very first frame won't show exactly as WinDBG, but it's close enough.
__declspec(noinline) unwind_context current_unwind_context() {
    return unwind_context{reinterpret_cast<uint64_t>(_AddressOfReturnAddress()), 0}.ret();
}

void default_print_address(out_stream& os, const IMAGE_DOS_HEADER&, uint64_t addr) {
    os << as_hex(addr);
}

} // unnamed namespace

//
// Unwind public
//

void print_stack(out_stream& os, find_image_function_type find_image, print_address_function_type print_address, int skip) {
    if (!print_address) print_address = &default_print_address;

    os << "Child-SP          RetAddr           Call Site\n";
    for (auto context = current_unwind_context(); context.rip;) {
        auto image = find_image(context.rip);
        if (!image) {
            os << "No image found for " << as_hex(context.rip) << "\n";
            break;
        }
        auto next = unwind_once(context, *image);
        if (!skip) {
            os << as_hex(context.rsp) << " " << as_hex(next.rip) << " ";
            print_address(os, *image, context.rip);
            os << "\n";
        } else {
            --skip;
        }
        context = next;
    }
}

} } // namespace attos::pe

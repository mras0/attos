#include "pe.h"
#include <attos/cpu.h>
#include <attos/out_stream.h>

#define SHOW_OPS_VERBOSE 0

namespace attos { namespace pe {

const char* const unwind_reg_names[16] = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8 ", "r9 ", "r10", "r11", "r12", "r13", "r14", "r15" };

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

namespace {
enum class unwind_frame_type {
    leaf, normal, iret
};

struct unwind_result {
    const uint64_t*   rsp;
    unwind_frame_type frame_type;
};

// Unwind the stack once. Returns `rsp' ready to pop the next return address
unwind_result unwind_once(const IMAGE_DOS_HEADER& image, uint64_t rip, const uint64_t* rsp) {
    const auto function_rva = static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(rip) - reinterpret_cast<const uint8_t*>(&image));
    const auto rf = runtime_function_info(image.data_directory<RUNTIME_FUNCTION>(), function_rva);
    if (rf == nullptr) {
#if SHOW_OPS_VERBOSE >= 1
        dbgout() << "    Assuming function at RVA " << as_hex(function_rva) << " (RIP = " << as_hex(rip) << ") is a leaf function\n";
#endif
        // If no function table entry is found, then it is in a leaf function, and RSP will directly address the return pointer.
        return { rsp, unwind_frame_type::leaf };
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
#if SHOW_OPS_VERBOSE >= 1
                dbgout() << "    pop " << unwind_reg_names[uc.OpInfo] << "\n";
#endif
                rsp += 1; // Pop
                break;
            case UWOP_ALLOC_LARGE:
            {
                REQUIRE(uc.OpInfo == 0); // No support for >512K-8 byte stacks, they use 3 nodes
                const auto qword_size = *reinterpret_cast<const uint16_t*>(&ui.unwind_codes()[++i]); // Consume unwind code
#if SHOW_OPS_VERBOSE >= 1
                dbgout() << "    add rsp, 0x" << as_hex(qword_size*8).width(0) << "\n";
#endif
                rsp += qword_size;
                break;
            }
            case UWOP_ALLOC_SMALL:
            {
                const auto qwords = (uc.OpInfo+1);
#if SHOW_OPS_VERBOSE >= 1
                dbgout() << "    add rsp, 0x" << as_hex(qwords*8).width(0) << "\n";
#endif
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
#if SHOW_OPS_VERBOSE >= 1
                dbgout() << "    mov " << unwind_reg_names[uc.OpInfo] << ", [rsp+0x" << as_hex(offset).width(0) << "]\n";
#endif
                break;
            }
            case UWOP_SAVE_NONVOL_FAR:
            case UWOP_SAVE_XMM128:
            case UWOP_SAVE_XMM128_FAR:
                //printf("Unhandled unwind OP %d\n", uc.UnwindOp);
                REQUIRE(false);
                break;
            case UWOP_PUSH_MACHFRAME:
                REQUIRE(uc.OpInfo == 0);
                // RIP, CS, EFLAGS, Old RSP, SS
#if SHOW_OPS_VERBOSE >= 1
                dbgout() << "    iretq ; !!!!\n";
#endif
                return { rsp, unwind_frame_type::iret };
        }
    }
    return { rsp, unwind_frame_type::normal };
}

inline uint64_t read_stack(uint64_t rsp, uint64_t byte_offset) {
    REQUIRE((rsp & 7) == 0);
    REQUIRE((byte_offset & 7) == 0);
    return *reinterpret_cast<const uint64_t*>(rsp + byte_offset);
}

class unwind_context {
public:
    uint64_t rsp() const { return rsp_; }
    uint64_t rip() const { return rip_; }

    unwind_context unwind() const {
        auto image = find_image_(rip());
        if (!image) {
            return unwind_context{0, 0, find_image_};
        }
        auto frame = unwind_once(*image, rip(), reinterpret_cast<const uint64_t*>(rsp()));

        if (frame.frame_type == unwind_frame_type::iret) {
            return { frame.rsp[3], frame.rsp[0], find_image_ };
        } else {
            REQUIRE(frame.frame_type == unwind_frame_type::normal || frame.frame_type == unwind_frame_type::leaf);
            auto next_rsp = reinterpret_cast<uint64_t>(frame.rsp);
            return { next_rsp + 8, read_stack(next_rsp, 0), find_image_ };
        }
    }

    // The very first from won't show exactly as WinDBG, but it's close enough.
    static unwind_context from_current_context(find_image_function_type find_image) {
        auto rsp_here = reinterpret_cast<uint64_t>(_AddressOfReturnAddress());
        return unwind_context{ rsp_here + 8, read_stack(rsp_here, 0), find_image };
    }

private:
    unwind_context(uint64_t rsp, uint64_t rip, find_image_function_type find_image) : rsp_(rsp), rip_(rip), find_image_(find_image) {
    }

    uint64_t rsp_;
    uint64_t rip_;
    find_image_function_type find_image_;
};

void default_print_address(out_stream& os, uint64_t addr) {
    os << as_hex(addr);
}

} // unnamed namespace

void print_stack(out_stream& os, find_image_function_type find_image, print_address_function_type print_address) {
    if (!print_address) print_address = &default_print_address;

    os << "Child-SP          RetAddr           Call Site\n";
    for (auto context = unwind_context::from_current_context(find_image); context.rip();) {
        auto next = context.unwind();
        os << as_hex(context.rsp()) << " " << as_hex(next.rip()) << " ";
        print_address(os, context.rip());
        os << "\n";
        context = next;
    }
}

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

} } // namespace attos::pe

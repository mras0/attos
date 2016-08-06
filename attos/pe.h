#ifndef ATTOS_PE_H
#define ATTOS_PE_H

#include <stdint.h>
#include <attos/array_view.h>

namespace attos {

class out_stream;

} // namespace attos

namespace attos { namespace pe {

struct IMAGE_NT_HEADERS;
struct IMAGE_SECTION_HEADER;
struct RUNTIME_FUNCTION;

namespace detail {
template<typename T, typename Y>
const T& rva_cast(const Y& from_obj, size_t offset) {
    auto p = reinterpret_cast<const uint8_t*>(&from_obj);
    //REQUIRE((uintptr_t)p >= (uintptr_t)base_);
    //REQUIRE((uintptr_t)p <= (uintptr_t)base_ + size_);
    //REQUIRE((uintptr_t)p + offset <= (uintptr_t)base_ + size_);
    //REQUIRE((uintptr_t)p + offset + sizeof(T) <= (uintptr_t)base_ + size_);
    return *reinterpret_cast<const T*>(p + offset);
}

} // namespace detail

#pragma pack(push, 1)
struct IMAGE_DOS_HEADER
{
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    uint32_t e_lfanew;

    template<typename T>
    const T& rva(uint64_t offset) const {
        return detail::rva_cast<T>(*this, offset);
    }

    const IMAGE_NT_HEADERS& nt_headers() const {
        return detail::rva_cast<IMAGE_NT_HEADERS>(*this, e_lfanew);
    }

    template<typename T>
    inline array_view<T> IMAGE_DOS_HEADER::data_directory() const {
        const auto& ioh = nt_headers().OptionalHeader;
        const auto& dd = ioh.DataDirectory[T::data_directory_index];
        auto base = &rva<T>(dd.VirtualAddress);
        return { base, base + dd.Size / sizeof(T) };
    }
};
constexpr uint16_t IMAGE_DOS_SIGNATURE = 0x5A4D; // MZ

struct IMAGE_FILE_HEADER {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
};
constexpr uint16_t IMAGE_FILE_MACHINE_I386  = 0x014c;
constexpr uint16_t IMAGE_FILE_MACHINE_AMD64 = 0x8664;

struct IMAGE_DATA_DIRECTORY {
    uint32_t VirtualAddress;
    uint32_t Size;
};

constexpr uint8_t IMAGE_DIRECTORY_ENTRY_EXPORT         =  0;  // Export Directory
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_IMPORT         =  1;  // Import Directory
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_RESOURCE       =  2;  // Resource Directory
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_EXCEPTION      =  3;  // Exception Directory
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_SECURITY       =  4;  // Security Directory
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_BASERELOC      =  5;  // Base Relocation Table
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_DEBUG          =  6;  // Debug Directory
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_COPYRIGHT      =  7;  // (X86 usage)
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_ARCHITECTURE   =  7;  // Architecture Specific Data
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_GLOBALPTR      =  8;  // RVA of GP
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_TLS            =  9;  // TLS Directory
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG    = 10;  // Load Configuration Directory
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT   = 11;  // Bound Import Directory in headers
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_IAT            = 12;  // Import Address Table
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT   = 13;  // Delay Load Import Descriptors
constexpr uint8_t IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR = 14;  // COM Runtime descriptor
constexpr uint8_t IMAGE_NUMBEROF_DIRECTORY_ENTRIES     = 16;  // Has changed through the years, check IMAGE_FILE_HEADER.SizeOfOptionalHeader

// Addresses area relative to image base. Structure must be DWORD aligned.
struct RUNTIME_FUNCTION {
    static constexpr uint8_t data_directory_index = IMAGE_DIRECTORY_ENTRY_EXCEPTION;

    uint32_t BeginAddress;
    uint32_t EndAddress;
    uint32_t UnwindInfoAddress;
};

struct UNWIND_CODE {
    uint8_t CodeOffset; // Offset in prolog
    uint8_t UnwindOp:4; // Unwind operation code
    uint8_t OpInfo:4;   // Operation information
};
static_assert(sizeof(UNWIND_CODE) == sizeof(uint16_t), "UNWIND_CODE has invalid size");

constexpr uint8_t UWOP_PUSH_NONVOL     =  0; // 1 node.    Push a nonvolatile integer register, decrementing RSP by 8. The operation info is the number of the register.
constexpr uint8_t UWOP_ALLOC_LARGE     =  1; // 2/3 nodes. Allocate a large-sized area on the stack.
constexpr uint8_t UWOP_ALLOC_SMALL     =  2; // 1 node.    Allocate a small-sized area on the stack.
constexpr uint8_t UWOP_SET_FPREG       =  3; // 1 node.    Establish the frame pointer register by setting the register to some offset of the current RSP.
constexpr uint8_t UWOP_SAVE_NONVOL     =  4; // 2 nodes.   Save a nonvolatile integer register on the stack using a MOV instead of a PUSH.
constexpr uint8_t UWOP_SAVE_NONVOL_FAR =  5; // 3 nodes.   Save a nonvolatile integer register on the stack with a long offset, using a MOV instead of a PUSH.
constexpr uint8_t UWOP_SAVE_XMM128     =  8; // 2 nodes.   Save all 128 bits of a nonvolatile XMM register on the stack.
constexpr uint8_t UWOP_SAVE_XMM128_FAR =  9; // 3 nodes.   Save all 128 bits of a nonvolatile XMM register on the stack with a long offset.
constexpr uint8_t UWOP_PUSH_MACHFRAME  = 10; // 1 node.    Push a machine frame.

constexpr uint8_t UWOP_REG_RAX   =  0;
constexpr uint8_t UWOP_REG_RCX   =  1;
constexpr uint8_t UWOP_REG_RDX   =  2;
constexpr uint8_t UWOP_REG_RBX   =  3;
constexpr uint8_t UWOP_REG_RSP   =  4;
constexpr uint8_t UWOP_REG_RBP   =  5;
constexpr uint8_t UWOP_REG_RSI   =  6;
constexpr uint8_t UWOP_REG_RDI   =  7;
constexpr uint8_t UWOP_REG_R8    =  8;
constexpr uint8_t UWOP_REG_R9    =  9;
constexpr uint8_t UWOP_REG_R10   = 10;
constexpr uint8_t UWOP_REG_R11   = 11;
constexpr uint8_t UWOP_REG_R12   = 12;
constexpr uint8_t UWOP_REG_R13   = 13;
constexpr uint8_t UWOP_REG_R14   = 14;
constexpr uint8_t UWOP_REG_R15   = 15;

constexpr uint8_t UWOP_REG_XMM0  =  0;
constexpr uint8_t UWOP_REG_XMM1  =  1;
constexpr uint8_t UWOP_REG_XMM2  =  2;
constexpr uint8_t UWOP_REG_XMM3  =  3;
constexpr uint8_t UWOP_REG_XMM4  =  4;
constexpr uint8_t UWOP_REG_XMM5  =  5;
constexpr uint8_t UWOP_REG_XMM6  =  6;
constexpr uint8_t UWOP_REG_XMM7  =  7;
constexpr uint8_t UWOP_REG_XMM8  =  8;
constexpr uint8_t UWOP_REG_XMM9  =  9;
constexpr uint8_t UWOP_REG_XMM10 = 10;
constexpr uint8_t UWOP_REG_XMM11 = 11;
constexpr uint8_t UWOP_REG_XMM12 = 12;
constexpr uint8_t UWOP_REG_XMM13 = 13;
constexpr uint8_t UWOP_REG_XMM14 = 14;
constexpr uint8_t UWOP_REG_XMM15 = 15;

extern const char* const unwind_reg_names[16];

struct UNWIND_INFO
{
    uint8_t Version:3;
    uint8_t Flags:5;
    uint8_t SizeOfProlog;
    uint8_t CountOfCodes;
    uint8_t FrameRegister:4;
    uint8_t FrameOffset:4;
//    UNWIND_CODE UnwindCode[CountOfCodes];

    array_view<UNWIND_CODE> unwind_codes() const {
        auto p = reinterpret_cast<const UNWIND_CODE*>(reinterpret_cast<const uint8_t*>(this) + sizeof(UNWIND_INFO));
        return { p, p + CountOfCodes };
    }
};
constexpr uint8_t UNWIND_INFO_VERSION        = 0x01;
constexpr uint8_t UNWIND_INFO_FLAG_EHANDLER  = 0x01;
constexpr uint8_t UNWIND_INFO_FLAG_UHANDLER  = 0x02;
constexpr uint8_t UNWIND_INFO_FLAG_CHAININFO = 0x04;

struct IMAGE_IMPORT_DESCRIPTOR {
    static constexpr uint8_t data_directory_index = IMAGE_DIRECTORY_ENTRY_IMPORT;

    union {
        uint32_t   Characteristics;            // 0 for terminating null import descriptor
        uint32_t   OriginalFirstThunk;         // RVA to original unbound IAT (PIMAGE_THUNK_DATA)
    };
    uint32_t   TimeDateStamp;                  // 0 if not bound,
                                               // -1 if bound, and real date\time stamp
                                               //     in IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT (new BIND)
                                               // O.W. date/time stamp of DLL bound to (Old BIND)

    uint32_t   ForwarderChain;                 // -1 if no forwarders
    uint32_t   Name;
    uint32_t   FirstThunk;                     // RVA to IAT (if bound this IAT has actual addresses)
};

struct IMAGE_OPTIONAL_HEADER64 {
    uint16_t Magic;
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
};
static_assert(sizeof(IMAGE_OPTIONAL_HEADER64) == 112 + IMAGE_NUMBEROF_DIRECTORY_ENTRIES * sizeof(IMAGE_DATA_DIRECTORY), "IMAGE_OPTIONAL_HEADER64 has wrong size");
constexpr uint16_t IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x20b;

struct IMAGE_NT_HEADERS {
    uint32_t                Signature;
    IMAGE_FILE_HEADER       FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;

    array_view<IMAGE_SECTION_HEADER> sections() const;
};
constexpr uint32_t IMAGE_NT_SIGNATURE = 0x00004550; // PE00

struct IMAGE_SECTION_HEADER {
    static constexpr uint8_t IMAGE_SIZEOF_SHORT_NAME = 8;

    uint8_t  Name[IMAGE_SIZEOF_SHORT_NAME];
    union {
        uint32_t PhysicalAddress;
        uint32_t VirtualSize;
    } Misc;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
};
constexpr uint32_t IMAGE_SCN_CNT_CODE               = 0x00000020; // The section contains executable code.
constexpr uint32_t IMAGE_SCN_CNT_INITIALIZED_DATA   = 0x00000040; // The section contains initialized data.
constexpr uint32_t IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080; // The section contains uninitialized data.
constexpr uint32_t IMAGE_SCN_MEM_EXECUTE            = 0x20000000; // The section can be executed as code.
constexpr uint32_t IMAGE_SCN_MEM_READ               = 0x40000000; // The section can be read.
constexpr uint32_t IMAGE_SCN_MEM_WRITE              = 0x80000000; // The section can be written to.

#pragma pack(pop)

inline array_view<IMAGE_SECTION_HEADER> IMAGE_NT_HEADERS::sections() const {
    auto begin = &detail::rva_cast<IMAGE_SECTION_HEADER>(OptionalHeader, FileHeader.SizeOfOptionalHeader);
    auto end   = begin + FileHeader.NumberOfSections;
    return {begin, end};
}

uint32_t file_size_from_header(const IMAGE_DOS_HEADER& image);

//
// Unwind
//
using find_image_function_type = const IMAGE_DOS_HEADER* (*)(uint64_t);
using print_address_function_type = void (out_stream&, const IMAGE_DOS_HEADER&, uint64_t);
void print_stack(out_stream& os, find_image_function_type find_image, print_address_function_type print_address, int skip);

} } // namespace attos::pe

#endif

#ifndef ATTOS_PE_H
#define ATTOS_PE_H

namespace attos { namespace pe {

struct IMAGE_NT_HEADERS;
struct IMAGE_SECTION_HEADER;

namespace detail {
template<typename T, typename Y>
const T& rva_cast(const Y& from_obj, size_t offset) {
    auto p = reinterpret_cast<const uint8_t*>(&from_obj);
    //CHECK((uintptr_t)p >= (uintptr_t)base_);
    //CHECK((uintptr_t)p <= (uintptr_t)base_ + size_);
    //CHECK((uintptr_t)p + offset <= (uintptr_t)base_ + size_);
    //CHECK((uintptr_t)p + offset + sizeof(T) <= (uintptr_t)base_ + size_);
    return *reinterpret_cast<const T*>(p + offset);
}

struct sections {
    const IMAGE_SECTION_HEADER* begin() const { return beg_; }
    const IMAGE_SECTION_HEADER* end()   const { return end_; }

    const IMAGE_SECTION_HEADER* beg_;
    const IMAGE_SECTION_HEADER* end_;
};


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
constexpr uint16_t IMAGE_FILE_MACHINE_I386 = 0x014c;
constexpr uint16_t IMAGE_FILE_MACHINE_AMD64 = 0x8664;

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
    //IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
};
constexpr uint16_t IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x20b;

struct IMAGE_NT_HEADERS {
    uint32_t                Signature;
    IMAGE_FILE_HEADER       FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;

    detail::sections sections() const;
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


inline detail::sections IMAGE_NT_HEADERS::sections() const {
    auto begin = &detail::rva_cast<IMAGE_SECTION_HEADER>(OptionalHeader, FileHeader.SizeOfOptionalHeader);
    auto end   = begin + FileHeader.NumberOfSections;
    return {begin, end};
}

} } // namespace attos::pe

#endif

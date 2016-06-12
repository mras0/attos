#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, #expr " failed\n"); exit(-1); } } while (0)

namespace {

void hexdump(const void* ptr, size_t len)
{
    if (!len) {
        printf("hexdump: empty\n");
        return;
    }
    const uintptr_t beg = reinterpret_cast<uintptr_t>(ptr);
    const uintptr_t end = beg + len - 1;
    for (uintptr_t a = beg & ~0xf; a < ((end+15) & ~0xf); a += 16) {
        for (unsigned i = 0; i < 16; i++) if (a+i >= beg && a+i <= end) printf("%02X ", *reinterpret_cast<const uint8_t*>(a+i)); else printf("   ");
        for (unsigned i = 0; i < 16; i++) {
            uint8_t c = ' ';
            if (a+i >= beg && a+i <= end) {
                uint8_t rc = *reinterpret_cast<const uint8_t*>(a+i);
                if (rc > ' ') c = rc; // poor mans isprint
            }
            printf("%c", c);
        }
        printf("\n");
    }
}

} // unnamed namespace

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

class pe_image {
public:
    explicit pe_image(const void* base, size_t size)
        : base_(reinterpret_cast<const uint8_t*>(base))
        , size_(size) {
        CHECK(size_ >= sizeof(IMAGE_DOS_HEADER));
        CHECK(dos_header().e_magic == IMAGE_DOS_SIGNATURE);
        CHECK(size_ >= dos_header().e_lfanew && size_ >= dos_header().e_lfanew + sizeof(IMAGE_NT_HEADERS));
        CHECK(nt_headers().Signature == IMAGE_NT_SIGNATURE);
        CHECK(file_header().Machine == IMAGE_FILE_MACHINE_AMD64);
        CHECK(optional_header().Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    }

    const IMAGE_DOS_HEADER& dos_header() const {
        return rva_cast<IMAGE_DOS_HEADER>(*base_, 0);
    }

    const IMAGE_NT_HEADERS& nt_headers() const {
        return rva_cast<IMAGE_NT_HEADERS>(dos_header(), dos_header().e_lfanew);
    }

    const IMAGE_FILE_HEADER& file_header() const {
        return nt_headers().FileHeader;
    }

    const IMAGE_OPTIONAL_HEADER64& optional_header() const {
        return nt_headers().OptionalHeader;
    }

    const IMAGE_SECTION_HEADER* sections() const {
        return &rva_cast<IMAGE_SECTION_HEADER>(optional_header(), file_header().SizeOfOptionalHeader);
    }

private:
    template<typename T, typename Y>
    const T& rva_cast(const Y& from_obj, size_t offset) const {
        auto p = reinterpret_cast<const uint8_t*>(&from_obj);
        CHECK((uintptr_t)p >= (uintptr_t)base_);
        CHECK((uintptr_t)p <= (uintptr_t)base_ + size_);
        CHECK((uintptr_t)p + offset <= (uintptr_t)base_ + size_);
        CHECK((uintptr_t)p + offset + sizeof(T) <= (uintptr_t)base_ + size_);
        return *reinterpret_cast<const T*>(p + offset);
    }

    const uint8_t* base_;
    size_t         size_;
};

void handle_pe(const pe_image& img)
{
    printf("SizeOfOptionalHeader = 0x%X\n", img.file_header().SizeOfOptionalHeader);
    printf("ImageBase = %llX\n", img.optional_header().ImageBase);

    auto ish = img.sections();
    for (int s = 0; s < img.file_header().NumberOfSections; ++s) {
        printf("Section #%d: %8.8s\n", s + 1, ish[s].Name);
#define P(name) printf("  %-30.30s 0x%8.8X\n", #name, ish[s] . name)
        P(Misc.VirtualSize);
        P(VirtualAddress);
        P(SizeOfRawData);
        P(PointerToRawData);
        P(PointerToRelocations);
        P(PointerToLinenumbers);
        P(NumberOfRelocations);
        P(NumberOfLinenumbers);
        P(Characteristics);
#undef P
#define C(f) if (ish[s].Characteristics & IMAGE_SCN_ ## f) printf("    " #f "\n")
        C(CNT_CODE);
        C(CNT_INITIALIZED_DATA);
        C(CNT_UNINITIALIZED_DATA);
        C(MEM_EXECUTE);
        C(MEM_READ);
        C(MEM_WRITE);
#undef C
    }
}

int main(int argc, const char* argv[])
{
    const char* const filename = argc >= 2 ? argv[1] : "../stage3/stage3.exe";
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error opening '%s'\n", filename);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    const auto size = static_cast<size_t>(ftell(fp));
    fseek(fp, 0, SEEK_SET);
    auto buf = malloc(size);
    fread(buf, size, 1, fp);
    fclose(fp);
    handle_pe(pe_image{buf, size});
    free(buf);
}

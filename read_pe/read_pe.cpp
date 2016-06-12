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

uint16_t le_u16(const uint8_t* buf)
{
    return buf[0] | (buf[1] << 8);
}

uint32_t le_u32(const uint8_t* buf)
{
    return le_u16(buf) | (le_u16(buf+2) << 16);
}

uint64_t le_u64(const uint8_t* buf)
{
    return le_u32(buf) | (static_cast<uint64_t>(le_u32(buf+4)) << 32);
}

#pragma pack(push, 1)
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
#pragma pack(pop)

} // unnamed namespace

void handle_pe(const uint8_t* pe_buf, size_t pe_size)
{
    // TODO: Check file offsets

    // IMAGE_DOS_HEADER
    constexpr uint16_t IMAGE_DOS_SIGNATURE       = 0x5A4D; // MZ
    constexpr uint16_t IMAGE_DOS_HEADER_size     = 0x40;
    constexpr uint16_t IMAGE_DOS_HEADER_e_lfanew = 0x3C;
    CHECK(pe_size >= IMAGE_DOS_HEADER_size);
    CHECK(le_u16(pe_buf) == IMAGE_DOS_SIGNATURE);
    const auto e_lfanew = le_u32(pe_buf + IMAGE_DOS_HEADER_e_lfanew);

    // IMAGE_NT_HEADERS
    constexpr uint16_t IMAGE_NT_SIGNATURE   = 0x00004550; // PE00
    constexpr uint16_t SIZE_OF_NT_SIGNATURE = 4;
    const auto inh = pe_buf + e_lfanew;
    CHECK(le_u32(inh) == IMAGE_NT_SIGNATURE);

    // IMAGE_FILE_HEADER
    constexpr auto IMAGE_SIZEOF_FILE_HEADER = 20;
    constexpr uint16_t IMAGE_FILE_MACHINE_I386 = 0x014c;
    constexpr uint16_t IMAGE_FILE_MACHINE_AMD64 = 0x8664;
    const auto ifh = inh + SIZE_OF_NT_SIGNATURE;
    CHECK(le_u16(ifh) == IMAGE_FILE_MACHINE_AMD64); // IMAGE_FILE_HEADER.Machine
    const auto number_of_sections = le_u16(ifh+2);// IMAGE_FILE_HEADER.NumberOfSections
    const auto size_of_optional_header = le_u16(ifh+16); // IMAGE_FILE_HEADER.SizeOfOptionalHeader
    printf("size_of_optional_header = 0x%X\n", size_of_optional_header);

    // IMAGE_OPTIONAL_HEADER
    const auto ioh = reinterpret_cast<const IMAGE_OPTIONAL_HEADER64*>(ifh + IMAGE_SIZEOF_FILE_HEADER);
    constexpr uint16_t IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x20b;
    CHECK(ioh->Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    printf("ImageBase = %llX offset=%d\n", ioh->ImageBase, (int)offsetof(IMAGE_OPTIONAL_HEADER64, ImageBase));

    // IMAGE_SECTION_HEADER
    const auto ish = reinterpret_cast<const uint8_t*>(ioh) + size_of_optional_header;
    constexpr auto IMAGE_SIZEOF_SECTION_HEADER = 40;
    constexpr auto IMAGE_SIZEOF_SHORT_NAME     = 8;

    for (int s = 0; s < number_of_sections; ++s) {
        auto sh = ish + s * IMAGE_SIZEOF_SECTION_HEADER;
        printf("Section #%d: %8.8s\n", s + 1, sh);
#define P(offset, name) printf("  " name " = 0x%lX\n", le_u32(sh + offset))
        P(0x08, "Misc");
        P(0x0C, "VirtualAddress");
        P(0x10, "SizeOfRawData");
        P(0x14, "PointerToRawData");
        P(0x18, "PointerToRelocations");
        P(0x1C, "PointerToLinenumbers");
        P(0x20, "Numbers");
        P(0x24, "Characteristics");
#undef P
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
    const auto size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    auto buf = malloc(size);
    fread(buf, size, 1, fp);
    fclose(fp);
    handle_pe(static_cast<const uint8_t*>(buf), size);
    free(buf);
}

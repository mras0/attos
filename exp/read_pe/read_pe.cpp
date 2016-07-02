#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <intrin.h>
#include <algorithm>

#include <attos/pe.h>
#include <attos/cpu.h>

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

namespace attos {
void fatal_error(const char* file, int line, const char* detail) {
    fprintf(stderr, "%s:%d: %s\n", file, line, detail);
    abort();
}

} // namespace attos

using namespace attos;
using namespace attos::pe;

void handle_pe(const IMAGE_DOS_HEADER& img)
{
    auto& ioh = img.nt_headers().OptionalHeader;
    printf("AddressOfEntryPoint = %x\n", ioh.AddressOfEntryPoint);
    printf("ImageBase           = %llx\n", ioh.ImageBase);
    printf("SectionAlignment    = %x\n", ioh.SectionAlignment);
    printf("FileAlignment       = %x\n", ioh.FileAlignment);
    printf("NumberOfRvaAndSizes = %u\n", ioh.NumberOfRvaAndSizes);

    auto ish = img.nt_headers().sections();
    for (const auto& s: ish) {
        printf("Section #%d: %8.8s\n", (int)(&s - ish.begin()) + 1, s.Name);
#define P(name) printf("  %-30.30s 0x%8.8x\n", #name, s . name)
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
#define C(f) if (s.Characteristics & IMAGE_SCN_ ## f) printf("    " #f "\n")
        C(CNT_CODE);
        C(CNT_INITIALIZED_DATA);
        C(CNT_UNINITIALIZED_DATA);
        C(MEM_EXECUTE);
        C(MEM_READ);
        C(MEM_WRITE);
#undef C
    }

    for (const auto& s: ish) {
        if (!s.SizeOfRawData) continue;
        uint64_t va  = ioh.ImageBase + s.VirtualAddress;
        uint32_t rel = s.VirtualAddress - s.PointerToRawData;
        printf("Map %8.8s 0x%08x`%08x to file offset 0x%08x [Relative 0x%x]\n", s.Name, (uint32_t)(va>>32), (uint32_t)va, s.PointerToRawData, rel);
    }
}

extern "C" IMAGE_DOS_HEADER __ImageBase;
extern "C" int GetModuleHandleExA(uint32_t flags, const void* module_name, void** module_handle);
constexpr uint32_t GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT = 0x00000002;
constexpr uint32_t GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS       = 0x00000004;
extern "C" uint32_t GetModuleFileNameA(const void* module, char* filename, uint32_t size);

#define FMT64   "%08x`%08x"
#define PR64(x) (uint32_t)(((uint64_t)(x)) >> 32), (uint32_t)(((uint64_t)(x)))

const IMAGE_DOS_HEADER& find_image(uint64_t virtual_address)
{
    const auto& ioh = __ImageBase.nt_headers().OptionalHeader;
    if (virtual_address >= ioh.ImageBase && virtual_address < ioh.ImageBase + ioh.SizeOfImage) {
        return __ImageBase;
    }

    //printf("Searching for image for " FMT64 "\n", PR64(virtual_address));

    void* module_handle;
    REQUIRE(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (void*)virtual_address, &module_handle));
    const auto& image = *reinterpret_cast<const IMAGE_DOS_HEADER*>(module_handle);
    REQUIRE(image.e_magic == IMAGE_DOS_SIGNATURE);
    return image;
}

// Warning: returns pointer to static buffer
const char* image_filename(const IMAGE_DOS_HEADER& image)
{
    REQUIRE(image.e_magic == IMAGE_DOS_SIGNATURE);
    static char filename[261];
    REQUIRE(GetModuleFileNameA((void*)&image, filename, sizeof(filename)) != 0);
    char* p = filename;
    // Move to end
    while (*p) p++;
    // Erase extension
    for (; p >= filename && *p != '\\'; --p) {
        const char old = *p;
        *p = 0;
        if (old == '.') break;
    }
    // Find start of filename
    while (p > filename && *(p-1) != '\\') --p;
    return p;
}

bool print_line(uint64_t child_rsp, uint64_t return_address, uint64_t rip)
{
    const auto& image = find_image(rip);
    printf(FMT64 " " FMT64 " %s!+0x%04x\n", PR64(child_rsp), PR64(return_address), image_filename(image), (uint32_t)(rip - (uint64_t)&image));
    return true;
}

void print_stack()
{
    uint64_t rip = []{ return reinterpret_cast<uint64_t>(_ReturnAddress()); }();
    auto child_rsp = reinterpret_cast<const uint64_t*>(_AddressOfReturnAddress());

    printf("Child-SP          RetAddr           Call Site\n");
    // Handle first tricky entry
    print_line(reinterpret_cast<uint64_t>(child_rsp), *child_rsp, rip);
    auto rsp = child_rsp;
    while (*rsp) {
        rip = *rsp;
        child_rsp = rsp + 1;
        rsp = unwind_once(find_image(rip), rip, rsp);
        print_line(reinterpret_cast<uint64_t>(child_rsp), *rsp, rip);
    }
}

int main(int argc, const char* argv[])
{
    const char* const filename = argc >= 2 ? argv[1] : argv[0];
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
    handle_pe(*reinterpret_cast<const IMAGE_DOS_HEADER*>(buf));
    free(buf);

    print_stack();
}

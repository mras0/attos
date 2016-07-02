#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <stdlib.h>
#include <stdint.h>
#include <intrin.h>
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

using namespace attos;
using namespace attos::pe;

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

void print_line(uint64_t child_rsp, uint64_t return_address, uint64_t rip)
{
    const auto& image = find_image(rip);
    printf(FMT64 " " FMT64 " %s!+0x%04x\n", PR64(child_rsp), PR64(return_address), image_filename(image), (uint32_t)(rip - (uint64_t)&image));
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
        rsp = unwind_once(find_image(rip), rip, rsp) + 1;
        print_line(reinterpret_cast<uint64_t>(child_rsp), *rsp, rip);
    }
}

int main()
{
    attos_stream_wrapper asw{std::cout};
    print_stack();
}

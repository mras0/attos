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
extern "C" void RaiseException(uint32_t code, uint32_t flags, uint32_t num_args, const uintptr_t* args);
extern "C" uint32_t GetLastError();
extern "C" unsigned long _exception_code();
constexpr uint32_t EXCEPTION_EXECUTE_HANDLER    =   1;
constexpr uint32_t EXCEPTION_CONTINUE_SEARCH    =   0;
constexpr uint32_t EXCEPTION_CONTINUE_EXECUTION = ~0U;


#define FMT64   "%08x`%08x"
#define PR64(x) (uint32_t)(((uint64_t)(x)) >> 32), (uint32_t)(((uint64_t)(x)))

const IMAGE_DOS_HEADER* find_image(uint64_t virtual_address)
{
    const auto& ioh = __ImageBase.nt_headers().OptionalHeader;
    if (virtual_address >= ioh.ImageBase && virtual_address < ioh.ImageBase + ioh.SizeOfImage) {
        return &__ImageBase;
    }

    //printf("Searching for image for " FMT64 "\n", PR64(virtual_address));

    void* module_handle;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (void*)virtual_address, &module_handle)) {
        const auto error = GetLastError();
        dbgout() << "GetModuleHandleExA failed for " << as_hex(virtual_address) << ": " << error << "\n";
    }
    auto image = reinterpret_cast<const IMAGE_DOS_HEADER*>(module_handle);
    REQUIRE(image->e_magic == IMAGE_DOS_SIGNATURE);
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

void print_address(out_stream& os, uint64_t rip)
{
    if (auto image = find_image(rip)) {
        os << image_filename(*image) << "!+0x" << as_hex(rip - (uint64_t)image).width(4);
    } else {
        os << as_hex(rip);
    }
}

extern "C" void test_fun(void);

constexpr uint32_t my_exception_code = 0x0000'FEDE; // WinDBG doesn't like exceptions that use the upper 16-bits of the exception code TODO: why?

extern "C" void foo(void) {
    print_stack(dbgout(), find_image, print_address);
    dbgout() << "\nRaising exception " << as_hex(my_exception_code) << "\n";
    RaiseException(my_exception_code, 0, 0, nullptr);
}

int main()
{
    attos_stream_wrapper asw{std::cout};
    __try {
        test_fun();
    } __except(_exception_code() == my_exception_code ?  EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        dbgout() << "Caught exception!\n";
        return 0;
    }
    dbgout() << "We didn't execute our handler!\n";
    abort();
}

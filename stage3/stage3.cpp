#include <stdint.h>
#include <intrin.h>
#include <type_traits>

#include <attos/mem.h>
#include <attos/pe.h>
#include <attos/vga/text_screen.h>

namespace attos {

out_stream* global_dbgout;

void set_dbgout(out_stream& os) {
    global_dbgout = &os;
}

out_stream& dbgout() {
    return *global_dbgout;
}

}  // namespace attos

using namespace attos;

const unsigned char bochs_magic_code[] = { 0x66, 0x87, 0xDB, 0xC3 }; // xchg bx, bx; ret
auto bochs_magic = ((void (*)(void))(void*)bochs_magic_code);

uint8_t read_key() {
    static constexpr uint8_t ps2_data_port    = 0x60;
    static constexpr uint8_t ps2_status_port  = 0x64; // when reading
    static constexpr uint8_t ps2_command_port = 0x64; // when writing
    static constexpr uint8_t ps2s_output_full = 0x01;
    static constexpr uint8_t ps2s_input_fill  = 0x02;

    while (!(__inbyte(ps2_status_port) & ps2s_output_full)) { // wait key
        __nop();
    }
    return __inbyte(ps2_data_port);  // read key
}

#pragma pack(push, 1)
struct smap_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
};
#pragma pack(pop)

struct arguments {
    const pe::IMAGE_DOS_HEADER&  image_base;
    smap_entry*                  smap_entries;
};

void print_page_tables()
{
    const auto cr3 = __readcr3();
    dbgout() << "cr3 = " << as_hex(cr3) << "\n";
    auto pml4 = (uint64_t*)cr3;
    for (int i = 0; i < 512; ++i ) {
        if (pml4[i] & PAGEF_PRESENT) {
            dbgout() << as_hex(i) << " " << as_hex(pml4[i]) << "\n";
            auto pdpt = (uint64_t*)(pml4[i]&~511);
            for (int j = 0; j < 512; ++j) {
                if (pdpt[j] & PAGEF_PRESENT) {
                    dbgout() << " " << as_hex(j) << " " << as_hex(pdpt[j]) << "\n";
                }
            }
        }
    }
}

void small_exe(const arguments& args)
{
    vga::text_screen ts;
    set_dbgout(ts);

    dbgout() << "Base             Length           Type\n";
    dbgout() << "FEDCBA9876543210 FEDCBA9876543210 76543210\n";
    for (auto e = args.smap_entries; e->type; ++e) {
        dbgout() << as_hex(e->base) << ' ' << as_hex(e->length) << ' ' << as_hex(e->type) << "\n";
    }

    print_page_tables();

    const auto& nth = args.image_base.nt_headers();
    for (const auto& s : nth.sections()) {
        for (auto c: s.Name) {
            dbgout() << char(c ? c : ' ');
        }
        dbgout() << " " << as_hex(s.VirtualAddress + nth.OptionalHeader.ImageBase) << " " << as_hex(s.Misc.VirtualSize) << "\n";
    }

    dbgout() << "Last mapped: " << as_hex(nth.OptionalHeader.ImageBase + nth.OptionalHeader.SizeOfImage + (1<<12)) << "\n";

    dbgout() << "Press any key to exit.\n";
    read_key();
}

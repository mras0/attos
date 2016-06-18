#include <stdint.h>
#include <intrin.h>
#include <type_traits>

#include <attos/mem.h>
#include <attos/vga/text_screen.h>

// http://www.catch22.net/tuts/reducing-executable-size
// Merge all default sections into the .text (code) section.
#pragma comment(linker,"/merge:.rdata=.data")
#pragma comment(linker,"/merge:.text=.data")
#pragma comment(linker,"/merge:.pdata=.data")
//#pragma comment(linker,"/merge:.reloc=.data")

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
    smap_entry* smap_entries;
};

void small_exe(const arguments& args)
{
    attos::vga::text_screen ts;
    using attos::as_hex;

    ts << "Base    Length   Type\n";
    for (auto e = args.smap_entries; e->type; ++e) {
        ts << as_hex(e->base) << ' ' << as_hex(e->length) << ' ' << as_hex(e->type) << "\n";
    }

    ts << "Press any key to exit.\n";
    uint8_t c;
    do {
        c = read_key();
        ts << "Key pressed: " << c << "\n";
    } while (!c);
}

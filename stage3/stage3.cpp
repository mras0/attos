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

void small_exe()
{
    attos::vga::text_screen ts;
    ts.clear();

    for (int i = 0; i < 30; ++i) {
        ts << "Hello world! Line " << i << "\n";
    }
    ts << INT64_MIN << "\n";

    ts << "Press any key to exit.\n";
    uint8_t c;
    do {
        c = read_key();
        ts << "Key pressed: " << c << "\n";
    } while (!c);
//    ((void (*)(void))(void*)bochs_magic_code)();
}

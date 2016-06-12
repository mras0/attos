// http://www.catch22.net/tuts/reducing-executable-size
// Merge all default sections into the .text (code) section.
#pragma comment(linker,"/merge:.rdata=.data")
#pragma comment(linker,"/merge:.text=.data")
#pragma comment(linker,"/merge:.pdata=.data")
//#pragma comment(linker,"/merge:.reloc=.data")

const unsigned char bochs_magic_code[] = { 0x66, 0x87, 0xDB, 0xC3 }; // xchg bx, bx; ret

void small_exe()
{
    unsigned short* const text = (unsigned short*)0xb8000;
    for (int i = 0; i < 80*25; ++i) {
        text[i] = static_cast<unsigned short>(0x4F00 | ('A' + i % 26));
    }
    ((void (*)(void))(void*)bochs_magic_code)();
}

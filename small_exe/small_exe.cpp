// http://www.catch22.net/tuts/reducing-executable-size
// Merge all default sections into the .text (code) section.
#pragma comment(linker,"/merge:.rdata=.data")
#pragma comment(linker,"/merge:.text=.data")
//#pragma comment(linker,"/merge:.reloc=.data")

void small_exe()
{
    *((unsigned short*)0xb8000) = 0x0700 | 'W';
    for (;;) {}
}

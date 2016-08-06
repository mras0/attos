#include <attos/out_stream.h>

using namespace attos;

int main()
{
    dbgout() << "Hello world from user mode answer=" << 42 << "\n";
    return 0xfede0abe;
}

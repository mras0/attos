#include <attos/out_stream.h>

using namespace attos;

extern "C" void syscall1(uint64_t n, uint64_t arg0);

int main()
{
    syscall1(1, (uint64_t)"Hello world from user mode!\n");
}

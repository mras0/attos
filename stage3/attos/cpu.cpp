#include "cpu.h"
#include <attos/out_stream.h>

namespace attos {

void fatal_error(const char* file, int line, const char* detail)
{
    dbgout() << file << ':' << line << ": " << detail << ".\nHanging\n";

    _disable();
    bochs_magic();
    for (;;) {
        __halt();
    }
}

} // namespace attos

#include "util.h"
#include <intrin.h>
#pragma intrinsic(_disable)
#pragma intrinsic(_enable)

namespace attos {

void halt() {
    _disable();
    bochs_magic();
    for (;;) {
        __halt();
    }
}

out_stream* global_dbgout;

void set_dbgout(out_stream& os) {
    global_dbgout = &os;
}

out_stream& dbgout() {
    return *global_dbgout;
}

} // namespace attos

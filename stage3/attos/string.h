#ifndef ATTOS_STRING_H
#define ATTOS_STRING_H

#include <stddef.h>

namespace attos {

inline size_t string_length(const char* s) {
    size_t len = 0;
    for (; *s; ++s) ++len;
    return len;
}

} // namespace attos

#endif

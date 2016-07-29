#ifndef ATTOS_STRING_H
#define ATTOS_STRING_H

#include <stddef.h>

extern "C" int memcmp(const void* p1, const void* p2, size_t count); // crt.asm

namespace attos {

inline size_t string_length(const char* s) {
    size_t len = 0;
    for (; *s; ++s) ++len;
    return len;
}

inline bool string_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return false;
        ++a;
        ++b;
    }
    return *a == *b;
}

} // namespace attos

#endif

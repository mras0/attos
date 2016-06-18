#ifndef ATTOS_MEM_H
#define ATTOS_MEM_H

#include <intrin.h>

namespace attos {

inline void move_memory(void* destination, const void* source, size_t count) {
    __movsb(reinterpret_cast<uint8_t*>(destination), reinterpret_cast<const uint8_t*>(source), count);
}

} // namespace attos

#endif

#include "mem.h"
#include <attos/out_stream.h>

namespace attos {

out_stream& operator<<(out_stream& os, memory_type type) {
    if (static_cast<uint32_t>(type & memory_type::read))          os << 'R';
    if (static_cast<uint32_t>(type & memory_type::write))         os << 'W';
    if (static_cast<uint32_t>(type & memory_type::execute))       os << 'X';
    if (static_cast<uint32_t>(type & memory_type::user))          os << 'U';
    if (static_cast<uint32_t>(type & memory_type::cache_disable)) os << 'C';
    if (static_cast<uint32_t>(type & memory_type::ps_2mb))        os << '2';
    if (static_cast<uint32_t>(type & memory_type::ps_1gb))        os << '1';
    return os;
}

} //namespace attos

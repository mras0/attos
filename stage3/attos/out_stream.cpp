#include "out_stream.h"

namespace attos {

out_stream& operator<<(out_stream& os, char c) {
    os.write(&c, 1);
    return os;
}

out_stream& operator<<(out_stream& os, const char* arg) {
    for (; *arg; ++arg) {
        os << *arg;
    }
    return os;
}

out_stream& operator<<(out_stream& os, uint64_t arg) {
    char buffer[64];
    int pos = sizeof(buffer);
    do {
        buffer[--pos] = (arg % 10) + '0';
        arg /= 10;
    } while (arg);
    os.write(&buffer[pos], sizeof(buffer) - pos);
    return os;
}

out_stream& operator<<(out_stream& os, uint32_t arg) {
    return os << static_cast<uint64_t>(arg);
}

out_stream& operator<<(out_stream& os, int64_t arg) {
    if (arg < 0) {
        return os << '-' << (static_cast<uint64_t>(arg ^ -1) + 1);
    }
    return os << static_cast<uint64_t>(arg);
}

out_stream& operator<<(out_stream& os, int32_t arg) {
    return os << static_cast<int64_t>(arg);
}

} // namespace attos

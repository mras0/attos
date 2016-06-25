#include "out_stream.h"

namespace attos {

namespace {

void format_number(out_stream& os, uint64_t number, unsigned base = 10, int min_width = 0, char fill = ' ')
{
    char buffer[64];
    int pos = sizeof(buffer);
    do {
        char c = static_cast<char>(number % base);
        buffer[--pos] = c + (c > 9 ? 'A' - 10 : '0');
        number /= base;
    } while (number);
    int count = sizeof(buffer) - pos;

    (void)fill;(void)min_width;
    while (count < min_width) {
        os.write(&fill, 1);
        min_width--;
    }
    os.write(&buffer[pos], count);
}

} // unnamed namespace

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
    format_number(os, arg);
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

out_stream& operator<<(out_stream& os, const formatted_number& fn) {
    format_number(os, fn.num(), fn.base(), fn.width(), fn.fill());
    return os;
}

void hexdump(out_stream& out, const void* ptr, size_t len) {
    if (len == 0) return;
    const uint64_t beg = reinterpret_cast<uint64_t>(ptr);
    const uint64_t end = beg + len;
    for (uint64_t a = beg & ~0xf; a < ((end+15) & ~0xf); a += 16) {
        for (uint32_t i = 0; i < 16; i++) {
            if (a+i >= beg && a+i <= end) {
                out << as_hex(*reinterpret_cast<const uint8_t*>(a+i));
            } else {
                out << "  ";
            }
            out << ' ';
        }
        for (uint32_t i = 0; i < 16; i++) {
            uint8_t c = ' ';
            if (a+i >= beg && a+i <= end) {
                uint8_t rc = *reinterpret_cast<const uint8_t*>(a+i);
                if (rc > ' ' && rc < 128) c = rc; // poor mans isprint
            }
            out << static_cast<char>(c);
        }
        out << '\n';
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

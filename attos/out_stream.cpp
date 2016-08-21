#include "out_stream.h"
#include <attos/string.h>

namespace attos {

namespace {

void format_number(out_stream& os, uint64_t number, unsigned base = 10, int min_width = 0, char fill = ' ')
{
    char buffer[64];
    int pos = sizeof(buffer);
    do {
        char c = static_cast<char>(number % base);
        buffer[--pos] = c + (c > 9 ? 'a' - 10 : '0');
        number /= base;
    } while (number);
    int count = sizeof(buffer) - pos;
    write_many(os, fill, min_width - count);
    os.write(&buffer[pos], count);
}

} // unnamed namespace

out_stream& operator<<(out_stream& os, char c) {
    os.write(&c, 1);
    return os;
}

out_stream& operator<<(out_stream& os, const char* arg) {
    os.write(arg, string_length(arg));
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

namespace detail {

out_stream& operator<<(out_stream& os, const formatted_string& fs) {
    const char* str = fs.str();
    int l = 0;
    for (; l < fs.max_width() && str[l]; ++l)
        ;
    os.write(str, l);
    if (fs.width() > l) {
        write_many(os, fs.fill(), fs.width() - l);
    }
    return os;
}

out_stream& operator<<(out_stream& os, const formatted_number& fn) {
    if (fn.base() == 16 && fn.width() == 16 && fn.fill() == '0') {
        format_number(os, fn.num()>>32, 16, 8, '0');
        os << '`';
        format_number(os, fn.num()&0xffffffff, 16, 8, '0');
    } else {
        format_number(os, fn.num(), fn.base(), fn.width(), fn.fill());
    }
    return os;
}

} // namespace detail

void write_many(out_stream& out, char c, int count) {
    while (--count >= 0) {
        out.write(&c, 1);
    }
}

void hexdump(out_stream& out, const void* ptr, size_t len) {
    if (len == 0) return;
    const uint64_t beg = reinterpret_cast<uint64_t>(ptr);
    const uint64_t end = beg + len;
    for (uint64_t a = beg & ~0xf; a < ((end+15) & ~0xf); a += 16) {
        for (uint32_t i = 0; i < 16; i++) {
            if (a+i >= beg && a+i < end) {
                out << as_hex(*reinterpret_cast<const uint8_t*>(a+i));
            } else {
                out << "  ";
            }
            out << ' ';
        }
        for (uint32_t i = 0; i < 16; i++) {
            uint8_t c = ' ';
            if (a+i >= beg && a+i < end) {
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

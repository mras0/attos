#ifndef ATTOS_OUT_STREAM_H
#define ATTOS_OUT_STREAM_H

#include <stdint.h>

namespace attos {

class __declspec(novtable) out_stream {
public:
    virtual void write(const void* data, size_t n) = 0;
};

out_stream& operator<<(out_stream& os, char c);
out_stream& operator<<(out_stream& os, const char* arg);
out_stream& operator<<(out_stream& os, uint64_t arg);
out_stream& operator<<(out_stream& os, uint32_t arg);
out_stream& operator<<(out_stream& os, int64_t arg);
out_stream& operator<<(out_stream& os, int32_t arg);

class formatted_number {
public:
    formatted_number(uint64_t num, uint32_t width, uint8_t base, char fill) : num_(num), width_(width), base_(base), fill_(fill) {
    }

    uint64_t num() const { return num_; }

    uint32_t width() const { return width_; }
    formatted_number& width(uint32_t w) { width_ = w; return *this; }

    uint8_t  base() const { return base_; }

    char     fill() const { return fill_; }

private:
    uint64_t num_;
    uint32_t width_;
    uint8_t  base_;
    char     fill_;
};

template<typename I>
formatted_number as_hex(I i) {
    return formatted_number{static_cast<uint64_t>(i), sizeof(I)*2, 16, '0'};
}

out_stream& operator<<(out_stream& os, const formatted_number& fn);

void hexdump(out_stream& out, const void* ptr, size_t len);

void set_dbgout(out_stream& os);
out_stream& dbgout();

} // namespace attos

#endif

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

template<typename Derived>
class formatted_base {
public:
    int width() const { return width_; }
    Derived& width(int w) { width_ = w; return static_cast<Derived&>(*this); }

    char     fill() const { return fill_; }

protected:
    explicit formatted_base(int width, char fill) : width_(width), fill_(fill) {
    }

private:
    int width_;
    char     fill_;
};

namespace detail {
class formatted_string : public formatted_base<formatted_string> {
public:
    explicit formatted_string(int width, char fill, const char* str) : formatted_base(width, fill), str_(str) {
    }

    const char* str() const { return str_; }

private:
    const char* str_;
};
out_stream& operator<<(out_stream& os, const formatted_string& fs);

class formatted_number : public formatted_base<formatted_number> {
public:
    explicit formatted_number(int width, char fill, uint64_t num, uint8_t base) : formatted_base(width, fill), num_(num), base_(base) {
    }

    uint64_t num() const { return num_; }
    uint8_t  base() const { return base_; }

private:
    uint64_t num_;
    uint8_t  base_;
};
out_stream& operator<<(out_stream& os, const formatted_number& fn);

} // namespace detail

template<typename I>
auto as_hex(I i) {
    return detail::formatted_number{sizeof(I)*2, '0', static_cast<uint64_t>(i), 16};
}

inline auto format_str(const char* str) {
    return detail::formatted_string{0, ' ', str};
}

void write_many(out_stream& out, char c, int count);

void hexdump(out_stream& out, const void* ptr, size_t len);

void set_dbgout(out_stream& os);
out_stream& dbgout();

} // namespace attos

#endif

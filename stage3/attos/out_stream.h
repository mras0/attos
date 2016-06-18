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

} // namespace attos

#endif

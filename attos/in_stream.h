#ifndef ATTOS_IN_STREAM_H
#define ATTOS_IN_STREAM_H

#include <stdint.h>

namespace attos {

class __declspec(novtable) in_stream {
public:
    virtual uint32_t read(void* data, uint32_t max) = 0;
};

} // namespace attos

#endif

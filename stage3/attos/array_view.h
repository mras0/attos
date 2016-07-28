#ifndef ATTOS_ARRAY_VIEW_H
#define ATTOS_ARRAY_VIEW_H

#include <stddef.h>

namespace attos {

template<typename T>
struct array_view {
public:
    array_view(const T* beg, const T* end) : beg_(beg), end_(end) {
    }

    size_t size() const { return end_ - beg_; }
    bool empty() const { return size() == 0; }

    const T* begin() const { return beg_; }
    const T* end()   const { return end_; }

    const T& operator[](size_t index) const { return *(beg_ + index); }

private:
    const T* beg_;
    const T* end_;
};

} // namespace attos

#endif

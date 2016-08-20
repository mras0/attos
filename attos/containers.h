#ifndef ATTOS_CONTAINERS_H
#define ATTOS_CONTAINERS_H

#include <attos/mem.h>

namespace attos {

void* kalloc(uint64_t size);
void kfree(void* ptr);

struct kfree_deleter {
    template<typename T>
    void operator()(T* ptr) {
        ptr->~T();
        kfree(ptr);
    }
};

template<typename T>
using kowned_ptr = owned_ptr<T, kfree_deleter>;

template<typename T, typename...Args >
auto knew(Args&&... args) {
    return kowned_ptr<T>{new (kalloc(sizeof(T))) T(static_cast<Args&&>(args)...)};
}

template<typename T>
class kvector {
public:
    explicit kvector() {
    }

    template<typename It>
    explicit kvector(It first, It last) {
        insert(end(), first, last);
    }

    kvector(kvector&& other) : begin_(other.begin_), end_(other.end_), real_end_(other.real_end_) {
        other.begin_    = nullptr;
        other.end_      = nullptr;
        other.real_end_ = nullptr;
    }

    kvector(const kvector&) = delete;

    ~kvector() {
        clear();
    }

    kvector& operator=(const kvector&) = delete;

    kvector& operator=(kvector&& other) {
        std::swap(begin_, other.begin_);
        std::swap(end_, other.end_);
        std::swap(real_end_, other.real_end_);
        return *this;
    }

    bool empty() const {
        return size() == 0;
    }

    size_t size() const {
        return end_ - begin_;
    }

    size_t capacity() const {
        return real_end_ - begin_;
    }

    void clear() {
        destroy_range(begin(), end());
        begin_ = end_ = real_end_ = nullptr;
    }

    void erase(T* elem) {
        elem->~T();
        while (elem + 1 < end_) {
            *elem = std::move(*(elem + 1));
            ++elem;
        }
        --end_;
    }

    void reserve(size_t new_capacity) {
        if (capacity() >= new_capacity) {
            return;
        }

        const auto old_size = size();
        T* new_ptr = reinterpret_cast<T*>(kalloc(sizeof(T) * new_capacity));
        for (auto it = begin(), it2 = new_ptr; it != end(); ++it, ++it2) {
            new (it2) T(std::move(*it));
        }
        destroy_range(begin(), end());
        begin_    = new_ptr;
        end_      = new_ptr + old_size;
        real_end_ = new_ptr + new_capacity;
    }

    T* begin() const { return begin_; }
    T* end() const { return end_; }

    T& front() const { return *begin_; }
    T& back() const { return *(end_-1); }

    const T& operator[](size_t index) const { return *(begin_ + index); }
    T& operator[](size_t index) { return *(begin_ + index); }

    void push_back(const T& elem) {
        ensure_room(size() + 1);
        new (end_) T(elem);
        ++end_;
    }

    void push_back(T&& elem) {
        //dbgout () << "push_back begin{ " << as_hex((uint64_t)begin_) << ", " << as_hex((uint64_t)end_) << ", " << as_hex((uint64_t)real_end_) << "}\n";
        ensure_room(size() + 1);
        //dbgout () << "push_back after reserve{ " << as_hex((uint64_t)begin_) << ", " << as_hex((uint64_t)end_) << ", " << as_hex((uint64_t)real_end_) << "}\n";
        new (end_) T(std::move(elem));
        //dbgout() << "push_back ok elem = " << as_hex((uint64_t)elem.get()) << ", end_ = " << as_hex((uint64_t)end_->get()) << "\n";
        ++end_;
    }

    void pop_back() {
        erase(end_-1);
    }

    template<typename It>
    void insert(T* where, It first, It last) {
        if (where != end()) __debugbreak(); // Lazy implementation
        const auto insert_count = last - first;
        ensure_room(size() + insert_count);
        static_assert(sizeof(*first) == sizeof(T), "Implementation is too lazy");
        static_assert(std::is_trivially_copyable_v<std::decay_t<decltype(*first)>>, "Implementation is too lazy");
        static_assert(std::is_trivially_copyable_v<T>, "Implementation is too lazy");
        memcpy(end_, first, insert_count * sizeof(T));
        end_ += insert_count;
    }

private:
    T* begin_    = nullptr;
    T* end_      = nullptr;
    T* real_end_ = nullptr;

    void ensure_room(size_t required_size) {
        //dbgout() << "ensure_room(" << required_size << ") capacity = " << capacity() << "\n";
        if (required_size > capacity()) {
            reserve(std::max(required_size, capacity() ? 2 * capacity() : 10));
        }
    }

    static void destroy_range(T* beg, T* end) {
        if (end != beg) {
            do {
                --end;
                end->~T();
            } while (end != beg);
        }
        if (beg) {
            kfree(beg);
        }
    }
};

} // nanmespace attos
#endif

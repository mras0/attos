#ifndef ATTOS_MM_H
#define ATTOS_MM_H

#include <attos/mem.h>

namespace attos {

class __declspec(novtable) memory_manager {
public:
    virtual ~memory_manager() {}

    static constexpr uint64_t page_size = 4096;

    physical_address pml4() const {
        return do_pml4();
    }

    void map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) {
        do_map_memory(virt, length, type, phys);
    }

private:
    virtual physical_address do_pml4() const = 0;
    virtual void do_map_memory(virtual_address virt, uint64_t length, memory_type type, physical_address phys) = 0;
};

owned_ptr<memory_manager, destruct_deleter> mm_init(physical_address base, uint64_t length);
void print_page_tables(physical_address pml4);

physical_address virt_to_phys(physical_address pml4, virtual_address virt);


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

    void reserve(size_t new_capacity) {
        if (capacity() >= new_capacity) {
            return;
        }

        const auto old_size = size();
        T* new_ptr = reinterpret_cast<T*>(kalloc(sizeof(T) * new_capacity));
        for (auto it = begin(), it2 = new_ptr; it != end(); ++it, ++it2) {
            *it2 = std::move(*it);
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
        if (end == beg) {
            return;
        }
        do {
            --end;
            end->~T();
        } while (end != beg);
        kfree(beg);
    }
};

} // namespace attos
#endif

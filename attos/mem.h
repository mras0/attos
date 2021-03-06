#ifndef ATTOS_MEM_H
#define ATTOS_MEM_H

#include <stdint.h>
#include <algorithm>

namespace attos {

constexpr uint64_t identity_map_start  = 0xFFFFFFFF'00000000;
constexpr uint64_t identity_map_length = 1<<30;

template<class T, uint64_t base>
constexpr T* fixed_physical_address = reinterpret_cast<T*>(base + identity_map_start);

#define ENUM_BIT_OP(type, op, inttype) \
constexpr inline type operator op(type l, type r) { return static_cast<type>(static_cast<inttype>(l) op static_cast<inttype>(r)); }
#define ENUM_BIT_OPS(type, inttype) \
    ENUM_BIT_OP(type, |, inttype)   \
    ENUM_BIT_OP(type, &, inttype)

enum class memory_type : uint32_t {
    read          = 0x0001,
    write         = 0x0002,
    execute       = 0x0004,
    user          = 0x0008,
    cache_disable = 0x0010,
    ps_2mb        = 0x1000,
    ps_1gb        = 0x2000,
};
class out_stream;
out_stream& operator<<(out_stream& os, memory_type type);

ENUM_BIT_OPS(memory_type, uint32_t)
constexpr auto memory_type_rw = memory_type::read | memory_type::write;
constexpr auto memory_type_rx = memory_type::read | memory_type::execute;
constexpr auto memory_type_rwx = memory_type::read | memory_type::write | memory_type::execute;

constexpr uint64_t memory_type_page_size(memory_type type) {
    return static_cast<uint32_t>(type & memory_type::ps_1gb) ? (1<<30) : static_cast<uint32_t>(type & memory_type::ps_2mb) ? (2<<20) : (1<<12);
}

template<typename T>
class address_base {
public:
    constexpr explicit address_base(uint64_t addr=0) : addr_(addr) {
    }

    constexpr operator uint64_t() const { return addr_; }

    address_base& operator+=(uint64_t rhs) {
        addr_ += rhs;
        return *this;
    }

protected:
    uint64_t addr_;
};

template<typename T>
constexpr T operator+(address_base<T> l, uint32_t r) {
    return T{static_cast<uint64_t>(l) + r};
}

template<typename T>
constexpr T operator+(address_base<T> l, uint64_t r) {
    return T{static_cast<uint64_t>(l) + r};
}

template<typename T>
constexpr T operator-(address_base<T> l, uint64_t r) {
    return T{static_cast<uint64_t>(l) - r};
}


template<typename T>
constexpr T operator&(address_base<T> l, uint64_t r) {
    return T{static_cast<uint64_t>(l) & r};
}

class virtual_address : public address_base<virtual_address> {
public:
    constexpr explicit virtual_address(uint64_t addr=0) : address_base(addr) {
    }

    static constexpr virtual_address in_current_address_space(const void* ptr) {
        return virtual_address{reinterpret_cast<uint64_t>(ptr)};
    }

    template<typename T = uint8_t>
    constexpr T* in_current_address_space() const {
        return reinterpret_cast<T*>(static_cast<uint64_t>(*this));
    }

    constexpr static uint64_t table_mask = 0x1FF;
    constexpr static uint32_t pml4_shift = 39;
    constexpr static uint32_t pdp_shift  = 30;
    constexpr static uint32_t pd_shift   = 21;
    constexpr static uint32_t pt_shift   = 12;

    constexpr uint32_t pml4e() const { return (addr_ >> pml4_shift) & table_mask; }
    constexpr uint32_t pdpe()  const { return (addr_ >> pdp_shift)  & table_mask; }
    constexpr uint32_t pde()   const { return (addr_ >> pd_shift)   & table_mask; }
    constexpr uint32_t pte()   const { return (addr_ >> pt_shift)   & table_mask; }
};

class physical_address : public address_base<physical_address> {
public:
    constexpr explicit physical_address(uint64_t addr=0) : address_base(addr) {
    }

    static constexpr physical_address from_identity_mapped_ptr(const void* ptr) {
        return physical_address{reinterpret_cast<uint64_t>(ptr) - identity_map_start};
    }

    template<typename T>
    constexpr operator T*() const { return reinterpret_cast<T*>(addr_ + identity_map_start); }
};


inline void move_memory(void* destination, const void* source, size_t count) {
    __movsb(reinterpret_cast<uint8_t*>(destination), reinterpret_cast<const uint8_t*>(source), count);
}

constexpr bool memory_areas_overlap(uint64_t start1, uint64_t len1, uint64_t start2, uint64_t len2) {
    return static_cast<int64_t>(std::min(start1 + len1, start2 + len2) - std::max(start1, start2)) > 0;
}

//  +------------------------------+--------+--------+------+-------+
//  | Name                         |  Entry | Maps   | Bits | Total |
//  +------------------------------+--------+--------+------+-------+
//  | Page Map Level 4             |  PML4E | 256 TB |    9 |    48 |
//  | Page Directory Pointer Table |  PDPE  | 512 GB |    9 |    39 |
//  | Page Directory Table         |  PDE   |   1 GB |    9 |    30 |
//  | Page Table                   |  PTE   |   2 MB |    9 |    21 |
//  | Each Page Table Entry        |        |   4 KB |   12 |    12 |
//  +------------------------------+--------+--------+------+-------+

#define PAGEF_PRESENT   0x0001    // P    Present
#define PAGEF_WRITE     0x0002    // R/W  Writeable
#define PAGEF_USER      0x0004    // U/S  User accessible
#define PAGEF_PWT       0x0008    // PWT  Page-level write through
#define PAGEF_PCD       0x0010    // PCD  Page-level cache disable
#define PAGEF_ACCESSED  0x0020    // A    Set by CPU
#define PAGEF_DIRTY     0x0040    // D    Set by CPU
#define PAGEF_PAGESIZE  0x0080    // PS   (for anything thats not a 4K PTE)
#define PAGEF_PAT_PTE   0x0080    // PAT  (for PTEs)
#define PAGEF_GLOBAL    0x0100    // G    Only valid in the lowest level
#define PAGEF_PAT_NPTE  0x1000    // PAT  Page-Attribute Table (only for non-PTEs)
#define PAGEF_NX       (1ULL<<63) // NX

                      //1<<9 through 1<<11 are availble to software
                      //as are 1<<52..1<<62

template<typename T, typename Deleter>
class owned_ptr {
public:
    explicit owned_ptr(T* ptr = nullptr) : ptr_(ptr) {
    }
    owned_ptr(owned_ptr&& other) : ptr_(other.release()) {
    }
    ~owned_ptr() {
        reset();
    }
    owned_ptr& operator=(owned_ptr&& rhs) {
        std::swap(ptr_, rhs.ptr_);
        return *this;
    }
    owned_ptr(owned_ptr& other) = delete;
    owned_ptr& operator=(owned_ptr& rhs) = delete;

    explicit operator bool() const {
        return ptr_ != nullptr;
    }

    T& operator*() const {
        return *get();
    }

    T* operator->() const {
        return get();
    }

    T* get() const {
        return ptr_;
    }

    void reset() {
        if (ptr_) Deleter()(ptr_);
        ptr_ = nullptr;
    }

    T* release() {
        auto ret = ptr_;
        ptr_ = nullptr;
        return ret;
    }

private:
    T* ptr_;
};

struct destruct_deleter {
    template<typename T>
    void operator()(T* ptr) {
        ptr->~T();
        (void)ptr; // Avoid msvc warning
    }
};

template<typename T>
class object_buffer {
public:
    object_buffer() = default;
    ~object_buffer() = default;
    object_buffer(const object_buffer&) = delete;
    object_buffer& operator=(const object_buffer&) = delete;

    template<typename... Args>
    auto construct(Args&&... args) {
        return owned_ptr<T, destruct_deleter>{new (buffer_) T(static_cast<Args&&>(args)...)};
    }
private:
    alignas(T) char buffer_[sizeof(T)];
};

template<typename T>
class singleton {
public:
    static bool has_instance() { return instance_ != nullptr; }
    static T& instance() { return *instance_; }

protected:
    explicit singleton() {
        if (instance_ != nullptr) __debugbreak();
        instance_ = static_cast<T*>(this);
    }

    ~singleton() {
        instance_ = nullptr;
    }
private:
    static T* instance_;
};
template<typename T>
T* singleton<T>::instance_;

// Simple heap. The free blocks are kept in list sorted according to memory address. Memory blocks are coalesced on free().
// No alignment is enformed and the user needs to know the size of allocations (or allocation memory to store them)
class simple_heap {
public:
    explicit simple_heap(uint8_t* base, uint64_t length);
    ~simple_heap();

    simple_heap(const simple_heap&) = delete;
    simple_heap& operator=(const simple_heap&) = delete;

    uint8_t* alloc(uint64_t size);
    void free(uint8_t* ptr, uint64_t size);

private:
    uint8_t* const base_;
    uint8_t* const end_;

    struct free_node {
        uint64_t   size;
        free_node* next;
    };
    static constexpr free_node* end_of_list = reinterpret_cast<free_node*>(~0ULL);
    free_node* free_;

    void insert_free(void* ptr, uint64_t size);
    static void coalesce_from(free_node* f);
};

class default_heap {
public:
    static constexpr uint64_t align = 16;

    explicit default_heap(uint8_t* base, uint64_t length) : heap_(base, length) {
    }

    void* alloc(uint64_t size);
    void free(void* ptr);
private:
    simple_heap heap_;
};

} // namespace attos

#endif

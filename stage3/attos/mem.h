#ifndef ATTOS_MEM_H
#define ATTOS_MEM_H

#include <stdint.h>
#include <algorithm>

namespace attos {

constexpr uint64_t identity_map_start  = 0xFFFFFFFF'00000000;
constexpr uint64_t identity_map_length = 1<<30;

template<class T, uint64_t base>
constexpr T* fixed_physical_address = reinterpret_cast<T*>(base + identity_map_start);

constexpr static uint32_t pml4_shift = 39;
constexpr static uint32_t pdp_shift  = 30;
constexpr static uint32_t pd_shift   = 21;
constexpr static uint32_t pt_shift   = 12;

constexpr static uint64_t table_mask = 0x1FF;

#define ENUM_BIT_OP(type, op, inttype) \
constexpr inline type operator op(type l, type r) { return static_cast<type>(static_cast<inttype>(l) op static_cast<inttype>(r)); }
#define ENUM_BIT_OPS(type, inttype) \
    ENUM_BIT_OP(type, |, inttype)   \
    ENUM_BIT_OP(type, &, inttype)

enum class memory_type : uint32_t {
    read    = 0x01,
    write   = 0x02,
    execute = 0x04,

    user    = 0x08,

    //ps_2mb  = 0x1000,
    ps_1gb  = 0x2000,
};

ENUM_BIT_OPS(memory_type, uint32_t)
constexpr auto memory_type_rwx = memory_type::read | memory_type::write | memory_type::execute;

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

class virtual_address : public address_base<virtual_address> {
public:
    constexpr explicit virtual_address(uint64_t addr=0) : address_base(addr) {
    }

    static constexpr virtual_address in_current_address_space(const void* ptr) {
        return virtual_address{reinterpret_cast<uint64_t>(ptr)};
    }

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

//  +------------------------------+--------+--------+------+
//  | Name                         |  Entry | Maps   | Bits |
//  +------------------------------+--------+--------+------+
//  | Page Map Level 4             |  PML4E | 256 TB |    9 |
//  | Page Directory Pointer Table |  PDPE  | 512 GB |    9 |
//  | Page Directory Table         |  PDE   |   1 GB |    9 |
//  | Page Table                   |  PTE   |   2 MB |    9 |
//  | Each Page Table Entry        |        |   4 KB |   12 |
//  +------------------------------+--------+--------+------+

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


} // namespace attos

#endif

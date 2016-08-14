#include "mem.h"
#include <attos/out_stream.h>
#include <attos/cpu.h>

namespace attos {

out_stream& operator<<(out_stream& os, memory_type type) {
    if (static_cast<uint32_t>(type & memory_type::read))          os << 'R';
    if (static_cast<uint32_t>(type & memory_type::write))         os << 'W';
    if (static_cast<uint32_t>(type & memory_type::execute))       os << 'X';
    if (static_cast<uint32_t>(type & memory_type::user))          os << 'U';
    if (static_cast<uint32_t>(type & memory_type::cache_disable)) os << 'C';
    if (static_cast<uint32_t>(type & memory_type::ps_2mb))        os << '2';
    if (static_cast<uint32_t>(type & memory_type::ps_1gb))        os << '1';
    return os;
}

simple_heap::simple_heap(uint8_t* base, uint64_t length) : base_(base), end_(base + length), free_(end_of_list) {
    REQUIRE(length >= sizeof(free_node));
    REQUIRE(length % sizeof(free_node) == 0);
    insert_free(base, length);
}

simple_heap::~simple_heap() {
    // All allocations should have been freed
    REQUIRE(free_ == reinterpret_cast<free_node*>(base_) && free_->size == static_cast<uint64_t>(end_ - base_));
}

uint8_t* simple_heap::alloc(uint64_t size) {
    //dbgout() << "simple_heap::alloc(" << as_hex(size) << ")\n";
    free_node** fp = &free_;
    while ((*fp) != end_of_list && (*fp)->size < size) {
        fp = &(*fp)->next;
    }
    REQUIRE(*fp != end_of_list && "Could not satisfy allocation request");

    uint8_t* res = reinterpret_cast<uint8_t*>(*fp);
    free_node* next_free     = *fp + size / sizeof(free_node);
    const uint64_t free_size = (*fp)->size - size;
    if (free_size) {
        next_free->size = free_size;
        next_free->next = (*fp)->next;
        //dbgout() << "simple_heap took from block " << **fp << " - new block " << *next_free << "\n";
        *fp = next_free;
    } else {
        //dbgout() << "simple_heap took complete block " << **fp << "\n";
        *fp = (*fp)->next;
    }
    //dbgout() << "simple_heap::alloc() returning " << as_hex((uint64_t)(res)) << "\n";
    return res;
}

void simple_heap::free(uint8_t* ptr, uint64_t size) {
    //dbgout() << "simple_heap::free(" << as_hex((uint64_t)ptr) << ") size = "  << as_hex(size) << "\n";
    REQUIRE((uint64_t)ptr >= (uint64_t)base_ && (uint64_t)ptr + size <= (uint64_t)end_);
    insert_free(ptr, size);
}

void simple_heap::insert_free(void* ptr, uint64_t size) {
    auto new_free = reinterpret_cast<free_node*>(ptr);
    new_free->size = size;
    new_free->next = end_of_list;

    // If the free list is empty or we're the new head
    if (free_ == end_of_list || free_ > new_free) {
        //dbgout() << "Inserting " << *new_free << " as new list head\n";
        new_free->next = free_;
        free_ = new_free;
        coalesce_from(free_);
        return;
    }

    // Find the node just before our insertion point
    free_node* prev = free_;
    for (; prev->next != end_of_list && prev->next < new_free; prev = prev->next)
        ;

    //dbgout() << "Inserting " << *new_free << " after " << *prev << " before ";
    //if (prev->next != end_of_list)
    //    dbgout() << *prev->next << "\n";
    //else
    //    dbgout() << "end_of_list\n";
    new_free->next = prev->next;
    prev->next = new_free;

    // Coalescing blocks starting from our insertion point
    coalesce_from(prev);
}

void simple_heap::coalesce_from(free_node* f) {
    while (f != end_of_list && f->next != end_of_list && reinterpret_cast<uint64_t>(f) + f->size == reinterpret_cast<uint64_t>(f->next)) {
        //dbgout() << "coalesce " << *f << " with " << *f->next << "\n";
        f->size += f->next->size;
        f->next  = f->next->next;
    }
}

void* default_heap::alloc(uint64_t size) {
    static_assert(align == 2*sizeof(uint64_t), "");
    size = round_up(size + align, align);
    auto ptr = heap_.alloc(size);
    reinterpret_cast<uint64_t*>(ptr)[0] = size; // Save size
    reinterpret_cast<uint64_t*>(ptr)[1] = reinterpret_cast<uint64_t>(ptr);  // Cookie to detect corruption
    return ptr + align;
}

void default_heap::free(void* ptr) {
    auto p = static_cast<uint8_t*>(ptr) - align;
    auto size = reinterpret_cast<const uint64_t*>(p)[0];
    REQUIRE(reinterpret_cast<const uint64_t*>(p)[1] == reinterpret_cast<uint64_t>(p)); // Check cookie
    reinterpret_cast<uint64_t*>(p)[1] = 0; // Clear cookie
    heap_.free(p, size);
}

} //namespace attos

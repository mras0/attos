#ifndef ATTOS_MEM_H
#define ATTOS_MEM_H

#include <intrin.h>

namespace attos {

constexpr uint64_t identity_map_start = 0xFFFFFFFF'00000000; // 1 GB mapped currently

inline void move_memory(void* destination, const void* source, size_t count) {
    __movsb(reinterpret_cast<uint8_t*>(destination), reinterpret_cast<const uint8_t*>(source), count);
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


} // namespace attos

#endif

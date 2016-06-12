    org 0x7e00
    bits 16

    jmp main

%include "asmutil.asm"

main:
    print_lit 'Hello world',13,10

    call a20_test

    call test_pmode

    call pe_test

exit:
    print_lit 13, 10, 'Done. Press any key to power off.', 13, 10
    ; Get key
    xor ax, ax
    int 0x16

    call power_off

hang:
    print_lit 13, 10, 'Hanging.', 13, 10
.halt:
    hlt
    jmp .halt

%include "a20.asm"
%include "poweroff.asm"
%include "pe.asm"

bits 16
%macro pmode_enter 1
    ; Disable interrupts
    cli

    ; Enable A20 Line
    mov al, 1
    call set_a20_status

    ; Load GDT
    lgdt [gdtr]

    ; Set CR0.PE = 1
    mov eax, cr0
    or al, 1
    mov cr0, eax

    ; Long jump to protected mode
    jmp 0x8:%1
%endmacro

test_pmode:
    push word .here
    pmode_enter test_pmode32
.here:
    print_lit 'Back in real mode!', 13, 10
    push word .here2
    print_lit 'Testing long mode...',13,10
    pmode_enter test_longmode
.here2:
    print_lit 'Back in real mode2!', 13, 10
    ret

    bits 32

test_pmode32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov edi, 0xb8000
    mov eax, 0x2F00 | '*'
    mov ecx, 80
    rep stosw
    jmp pmode_leave

%define EFER 0xc0000080

%macro set_page_entry 3 ; %1 = u64* dest, %2 = u32 hi, %3 = u32 lo
    mov     dword [%1], %3
    or      dword [%1], 3; PAGE_PRESENT | PAGE_WRITE
    mov     dword [%1+4], %2
%endmacro

test_longmode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; build initial page mapping (2MB identity mapped)
    set_page_entry pml4, 0, pdpt0
    ;set_page_entry initial_pml4+0x1FF*8, 0, pdptkrnl
    set_page_entry pdpt0, 0, pdt0
    ;set_page_entry pdptkrnl+0x1FE*8, 0, pdt0
    set_page_entry pdt0, 0, pt0
    mov     edi, pt0
    mov     esi, 0
    mov     ecx, 4096/8
.initpage:
    set_page_entry edi, 0, esi
    add     esi, 4096
    add     edi, 8
    dec     ecx
    jnz     .initpage

    ; Enable PAE (CR4.PAE=1)
    mov     eax, cr4
    bts     eax, 5
    mov     cr4, eax

    ; Load CR3 with PML4
    mov     eax, pml4
    mov     cr3, eax

    ; Enable long mode (EFER.LME=1)
    mov     ecx, EFER
    rdmsr
    bts     eax, 8
    wrmsr

    ; Enable paging (CR0.PG=1) -> Activates long mode (EFER.LMA=1)
    mov     eax, cr0
    bts     eax, 31
    mov     cr0, eax

    ; Jump to 64-bit
    jmp    0x28:.code64

    bits 64
.code64:
    mov ax, 0x30
    mov ds, ax
    mov es, ax
    mov edi, 0xb8000  + 80*2
    mov eax, 0x1F00 | 'X'
    mov ecx, 80
    rep stosw

    ; AMD24593 - AMD64 Architecture Programmer's Manual Volume 2: System Programming, 14.7 Leaving Long Mode
    ; 1. Switch to compatibility mode and place the processor at the highest privilege level (CPL=0).
.longmode_leave:
    cli
    mov rax, rsp
    push 0x10     ; ss
    push rax      ; rsp
    push 0x2      ; eflags (bit 1 is always set)
    push 0x08     ; cs
    push .leave32 ; rip
    iretq

    bits 32
.leave32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov edi, 0xb8000  + 80*2*2
    mov eax, 0x5F00 | 'Y'
    mov ecx, 80
    rep stosw

    bochs_magic
    ; 2. Deactivate long mode by clearing CR0.PG to 0. This causes the processor to clear the LMA bit to 0.
    mov     eax, cr0
    btc     eax, 31
    mov     cr0, eax
    ; 3. Load CR3 with the physical base-address of the legacy page tables.
    ; 4. Disable long mode by clearing EFER.LME to 0.
    mov     ecx, EFER
    rdmsr
    btc     eax, 8
    wrmsr

    jmp pmode_leave

    bits 32
pmode_leave:
    ; Disable interrupts, paging and ensure 16-bit prototected mode selectors are avilable
    cli
    ; Jump to protected mode
    jmp 0x18:.mode16

    bits 16
.mode16:
    ; Reload segments
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov ss, ax
    ; Reload real mode compatible IDT
    ; Disable protected mode
    mov eax, cr0
    and al, 0xfe
    mov cr0, eax
    ; jump to real mode
    jmp 0x0:.realmode
.realmode:
    ; Reload selectors
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    sti
    ret

GDTF_RW         equ 0x0002 ; For code segments RW=1 means the segment is readable, for data segments RW=1 means the segment is writable
GDTF_EXECUTABLE equ 0x0008
GDTF_SYSTEM     equ 0x0010 ; When the S (descriptor type) flag in a segment descriptor is clear, the descriptor type is a system descriptor (IA32 Vol. 3A 3.5):w
GDTF_PRESENT    equ 0x0080
GDTF_32BIT      equ 0x4000 ; Sz=0 16-bit, Sz=1 32-bit
GDTF_PAGE_GRAN  equ 0x8000 ; Granularity Gr=0 means byte granularity, Gr=1 means page (4K) granularity

%define GDT_ENTRY(base, limit, flags) ((limit&0xFFFF) | ((base & 0xFFFFFF) << 16) | ((flags & 0xF0FF) << 40) | ((limit>>16) << 48) | ((base>>24) << 56))

GDT_ENTRY_NULL   equ GDT_ENTRY(0, 0x0, 0x0)
GDT_ENTRY_CODE16 equ GDT_ENTRY(0, 0xFFFFF, GDTF_EXECUTABLE | GDTF_RW | GDTF_SYSTEM | GDTF_PRESENT)
GDT_ENTRY_DATA16 equ GDT_ENTRY(0, 0xFFFFF, GDTF_RW | GDTF_PRESENT | GDTF_SYSTEM)
GDT_ENTRY_CODE32 equ GDT_ENTRY(0, 0xFFFFF, GDTF_EXECUTABLE | GDTF_RW | GDTF_SYSTEM | GDTF_PRESENT | GDTF_32BIT | GDTF_PAGE_GRAN) ; code X executable R (readable) P (present) 4k Gran. 32-bit Ring0
GDT_ENTRY_DATA32 equ GDT_ENTRY(0, 0xFFFFF, GDTF_RW | GDTF_PRESENT | GDTF_SYSTEM | GDTF_32BIT | GDTF_PAGE_GRAN) ; data W writable  P (present) 4k Gran. 32-bit Ring0
GDT_ENTRY_CODE64 equ 0x0020980000000000
GDT_ENTRY_DATA64 equ 0x0000900000000000

gdt:
    dq GDT_ENTRY_NULL
    dq GDT_ENTRY_CODE32 ; 0x00CF9A000000FFFF
    dq GDT_ENTRY_DATA32 ; 0x00CF92000000FFFF
    dq GDT_ENTRY_CODE16
    dq GDT_ENTRY_DATA16
    dq GDT_ENTRY_CODE64
    dq GDT_ENTRY_DATA64
gdt_end:

gdtr:
    dw gdt_end - gdt - 1
    dd gdt

    align 4096 ; page tables must be 4K aligned
pml4     times 4096 db 0 ; Page Map Level 4
pdpt0    times 4096 db 0 ; First Directory Pointer Table
pdt0     times 4096 db 0 ; 0 Page Directory Table
pt0      times 4096 db 0 ; Page table for identity mapping the first 2MB
pdptkrnl times 4096 db 0 ; Kernel Directory Pointer Table

;
; End
;

    times (16*63*512)-($-$$) db 0 ; Make sure we fill at least one cylinder

    align 512, db 0 ; Pad to sector size

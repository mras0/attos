    org 0x7e00
    bits 16

    jmp main

%include "asmutil.asm"

main:
    print_lit 'Hello world',13,10

    call a20_test

    call test_pmode

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

bits 16
test_pmode:
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

    ; Long jump to protocted mode
    jmp 0x8:test_pmode32

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

.hang:
    jmp .hang

leave_pmode:
    ; 1. Disable interrupts
    cli
    ; 2. Turn of paging
    ; 3. 

    bits 16

GDTF_RW         equ 0x0002 ; For code segments RW=1 means the segment is readable, for data segments RW=1 means the segment is writable
GDTF_EXECUTABLE equ 0x0008
GDTF_SYSTEM     equ 0x0010 ; When the S (descriptor type) flag in a segment descriptor is clear, the descriptor type is a system descriptor (IA32 Vol. 3A 3.5):w
GDTF_PRESENT    equ 0x0080
GDTF_32BIT      equ 0x4000 ; Sz=0 16-bit, Sz=1 32-bit
GDTF_PAGE_GRAN  equ 0x8000 ; Granularity Gr=0 means byte granularity, Gr=1 means page (4K) granularity

%define GDT_ENTRY(base, limit, flags) ((limit&0xFFFF) | ((base & 0xFFFFFF) << 16) | ((flags & 0xF0FF) << 40) | ((limit>>16) << 48) | ((base>>24) << 56))

GDT_ENTRY_NULL   equ GDT_ENTRY(0, 0x0, 0x0)
GDT_ENTRY_CODE32 equ GDT_ENTRY(0, 0xFFFFF, GDTF_EXECUTABLE | GDTF_RW | GDTF_SYSTEM | GDTF_PRESENT | GDTF_32BIT | GDTF_PAGE_GRAN) ; code X executable R (readable) P (present) 4k Gran. 32-bit Ring0
GDT_ENTRY_DATA32 equ GDT_ENTRY(0, 0xFFFFF, GDTF_RW | GDTF_PRESENT | GDTF_SYSTEM | GDTF_32BIT | GDTF_PAGE_GRAN) ; data W writable  P (present) 4k Gran. 32-bit Ring0

gdt:
    dq GDT_ENTRY_NULL   ; 0x0000000000000000
    dq GDT_ENTRY_CODE32 ; 0x00CF9A000000FFFF
    dq GDT_ENTRY_DATA32 ; 0x00CF92000000FFFF
gdt_end:

gdtr:
    dw gdt_end - gdt - 1
    dd gdt

;
; End
;

    times (16*63*512)-($-$$) db 0 ; Make sure we fill at least one cylinder

    align 512, db 0 ; Pad to sector size

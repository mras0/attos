%include "bootcommon.inc"

    org LOAD_ADDR
    bits 16

    jmp main

%include "asmutil.asm"

main:
    print_lit 'Hello world',13,10

    call smap_init ; get system memory map

    ; call a20_test ; Disabling A20 is not supported by x200s?

    call pe_test

    ; call smap_print

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
%include "pe.asm"
%include "smap.asm"

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

%define PAGEF_PRESENT  0x01
%define PAGEF_WRITE    0x02
%define PAGEF_PAGESIZE 0x80

%macro set_page_entry 3 ; %1 = u64* dest, %2 = u32 hi, %3 = u32 lo
    mov     dword [%1], %3
    or      dword [%1], PAGEF_PRESENT | PAGEF_WRITE
    mov     dword [%1+4], %2
%endmacro

%define IDENTITY_MAP_START 0xFFFFFFFF00000000

test_longmode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; clear scratch area
    mov edi, SCRATCH_ADDRESS
    mov ecx, SCRATCH_SIZE/4
    xor eax, eax
    rep stosd

    ; build initial page mapping
    set_page_entry pml4, 0, pdpt0
    set_page_entry pml4+((IDENTITY_MAP_START>>39)&511)*8, 0, pdpt511
    ; Identity map 1GB physical 0 at 0xFFFFFFFF`00000000 and 0x00000000`00000000
    set_page_entry pdpt0, 0, pdt_id
    set_page_entry pdpt511+((IDENTITY_MAP_START>>30)&511)*8, 0, pdt_id
    mov edi, pdt_id
    mov esi, 0 | PAGEF_PAGESIZE
    mov ecx, 512
.initidmap:
    set_page_entry edi, 0, esi
    add esi, 2<<20
    add edi, 8
    dec ecx
    jnz .initidmap

    mov esi, stage3
    add esi, [esi+IMAGE_DOS_HEADER.e_lfanew]
    ; esi = IMAGE_NT_HEADERS*

    ; determine size by finding the section with the largest physical extend
    movzx edi, word [esi+IMAGE_NT_HEADERS.FileHeader+IMAGE_FILE_HEADER.SizeOfOptionalHeader]
    lea edi, [edi+esi+IMAGE_NT_HEADERS.OptionalHeader]
    ; edi = IMAGE_SECTION_HEADER*
    xor ecx, ecx ; ecx = largest extend so far
    movzx eax, word [esi+IMAGE_NT_HEADERS.FileHeader+IMAGE_FILE_HEADER.NumberOfSections]
.sections: ; eax = number of remaining sections to process
    and eax, eax
    jz .sectionsdone
    mov ebx, [edi+IMAGE_SECTION_HEADER.PointerToRawData]
    add ebx, [edi+IMAGE_SECTION_HEADER.SizeOfRawData]
    cmp ebx, ecx
    jbe .nextsection
    mov ecx, ebx
.nextsection:
    add edi, IMAGE_SECTION_HEADER_size
    dec eax
    jnz .sections
.sectionsdone:

    ; copy stage3 to new position
    pushad
    mov edi, stage3_copy
    mov esi, stage3
    rep movsb
    popad

    ; map stage3
    ; TODO: Handle sections, e.g. initialize BSS...
    ; Current limitations: Must not overlap identity mapping. ImageBase+SizeOfImage must not cross a 2MB boundary, Section mapping must be 4K aligned, etc..
    mov eax, [esi+IMAGE_NT_HEADERS.OptionalHeader+IMAGE_OPTIONAL_HEADER64.ImageBase]
    mov edx, [esi+IMAGE_NT_HEADERS.OptionalHeader+IMAGE_OPTIONAL_HEADER64.ImageBase+4]
    ; edx:eax = ImageBase
    ; PML4
    shrd eax, edx, 12 ; eax = (uint32_t)(ImageBase>>12)
    shr edx, 7        ; edx = (uint32_t)(ImageBase>>39) as shrld doesn't change edx
    and edx, 511
    shl edx, 3
    add edx, pml4
    ;; pdpt_program assumed to be pdpt511
    ;; set_page_entry edx, 0, pdpt_program
    ; PDPT
    mov edx, eax      ; edx = (uint32_t)(ImageBase>>12)
    shr edx, 30-12    ; edx = (uint32_t)(ImageBase>>30)
    and edx, 511
    shl edx, 3
    ;; pdpt_program assumed to be pdpt511
    ;;add edx, pdpt_program
    add edx, pdpt511
    set_page_entry edx, 0, pdt_program
    ; PDT
    mov edx, eax      ; edx = (uint32_t)(ImageBase>>12)
    shr edx, 21-12    ; edx = (uint32_t)(ImageBase>>21)
    and edx, 511
    shl edx, 3
    add edx, pdt_program
    set_page_entry edx, 0, pt_program
    ; PT
    mov ecx, [esi+IMAGE_NT_HEADERS.OptionalHeader+IMAGE_OPTIONAL_HEADER64.SizeOfImage]
    shr ecx, 12
    mov edi, pt_program
    mov esi, stage3_copy
.initpage2:
    set_page_entry edi, 0, esi
    add esi, 4096
    add edi, 8
    dec ecx
    jnz .initpage2

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

    ; Enable SSE
    mov rax, cr0
    btr rax, 2 ; Clear CR0.EM (co-processor EMulation)
    bts rax, 1 ; Set CR0.MP (Monitor co-Processor)
    mov cr0, rax

    mov rax, cr4
    or rax, 3 << 9 ; Set CR4.OSFXSR, CR4.OSXMMEXCPT
    mov cr4, rax

    ; Make sure the stack is acceptable
    xor rbp, rbp  ; this is the start of the frame pointer chain
    push rbp
    mov rbp, rsp  ; preserve old stack pointer

    ; build argument structure (stage3 knows that the pointers are physical addresses)
    sub rsp, 0x18
    mov rcx, rsp
    mov qword [rcx+0x00], stage3_copy
    mov qword [rcx+0x08], stage3
    mov qword [rcx+0x10], smap_buffer

    mov rax, IDENTITY_MAP_START
    add rcx, rax
    add rsp, rax  ; Stack pointer should point to identity
    and rsp, -16  ; in 64-bit the stack must be 16 byte aligned before a call
    sub rsp, 0x20 ; make room for the function to preserve rcx, rdx, r8 and r9


    mov esi, stage3_copy
    add esi, [rsi+IMAGE_DOS_HEADER.e_lfanew]
    ; rsi = IMAGE_NT_HEADERS*
    mov eax, [rsi+IMAGE_NT_HEADERS.OptionalHeader+IMAGE_OPTIONAL_HEADER64.AddressOfEntryPoint]
    add rax, [rsi+IMAGE_NT_HEADERS.OptionalHeader+IMAGE_OPTIONAL_HEADER64.ImageBase]
    call rax

    ; Restore stack pointer
    mov rsp, rbp
    pop rbp

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

;
; DATA
;

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

    align 4096
stage3: ; incbin "../stage3/stage3.exe"

;
; BSS
;

;
; +------------------------------+--------+------+
; | Name                         | Maps   | Bits |
; +------------------------------+--------+------+ // 48 0xffffffffffffffff
; | Page Map Level 4             | 256 TB |    9 | // 39 0x0000ff8000000000
; | Page Directory Pointer Table | 512 GB |    9 | // 30 0x0000007fc0000000
; | Page Directory Table         |   1 GB |    9 | // 21 0x000000003fe00000
; | Page Table                   |   2 MB |    9 | // 12 0x00000000001ff000
; | Each Page Table Entry        |   4 KB |   12 | //  0 0x0000000000000fff
; +------------------------------+--------+------+
;

    absolute SCRATCH_ADDRESS
    align 4096, resb 1 ; page tables must be 4K aligned
pml4         resb 4096 ; Page Map Level 4
pdpt0        resb 4096 ; First Directory Pointer Table
pdpt511      resb 4096 ; Last Directory Pointer Table
pdt_id       resb 4096 ; Identity Page Directory Table
pdt_program  resb 4096 ; Program Page Directory Table
pt_program   resb 4096 ; Program Page Table

    align 4096, resb 1
stage3_copy:

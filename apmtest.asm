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
.spin:
    jmp .spin

    bits 16

gdt:
    dq      0x0000000000000000 ; null
    dq      0x00CF9A000000FFFF ; code X executable R (readable) P (present) 4k Gran. 32-bit Ring0
    dq      0x00CF92000000FFFF ; data W writable  P (present) 4k Gran. 32-bit Ring0
gdt_end:

gdtr:
    dw gdt_end - gdt - 1
    dd gdt

;
; End
;

    times (16*63*512)-($-$$) db 0 ; Make sure we fill at least one cylinder

    align 512, db 0 ; Pad to sector size

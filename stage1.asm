    BOOT_ADDR equ 0x7c00
    LOAD_ADDR equ 0x7e00

    MAX_SECTORS equ 0x7f

    ;
    ; Init code
    ;

    org BOOT_ADDR
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, BOOT_ADDR
    jmp 0:main

%include "asmutil.asm"

main:
    mov [boot_drive], dl
    print_lit 'Booting from drive 0x'
    mov al, [boot_drive]
    call put_hex8
    call put_crlf

    ; Get Drive Parameters
    mov ah, 0x48
    mov dl, [boot_drive]
    mov si, LOAD_ADDR ; use loading area for buffer
    mov word [si], 0x1A
    int 0x13
    jc failed

    print_lit 'Total number of sectors: 0x'
    mov si, LOAD_ADDR+0x10
    call put_hex64
    call put_crlf

    ; Use sector count - 1 as count to load (though or at most MAX_SECTORS)
    cmp word [LOAD_ADDR+0x16], 0
    jne .toolarge
    cmp word [LOAD_ADDR+0x14], 0
    jne .toolarge
    cmp word [LOAD_ADDR+0x12], 0
    jne .toolarge
    mov ax, [LOAD_ADDR+0x10]
    dec ax
    cmp ax, MAX_SECTORS
    jle .sizeok
.toolarge:
    mov ax, MAX_SECTORS
.sizeok:
    mov word [address_packet_block_count], ax

    print_lit 'Reading 0x'
    call put_hex16
    print_lit ' sectors...'

    ; Extended Read
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, address_packet
    int 0x13
    jc failed

    print_lit 'OK.',13,10
    jmp LOAD_ADDR

failed:
    print_lit 'Failed. AX = 0x'
    call put_hex16
    ; fall through
hang:
    hlt
    jmp hang

address_packet:
    db 0x10                 ; size
    db 0                    ; reserved
address_packet_block_count:
    dw 1                    ; number of blocks
    dw 0                    ; buffer (offset)
    dw LOAD_ADDR>>4         ; buffer (segment)
    dq 1                    ; starting absolute block number

    times 509-($-$$) db 0
boot_drive db 0
    dw 0xaa55

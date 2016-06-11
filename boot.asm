    mov ax, 0x13
    int 0x10
    mov ax, 0xa000
    mov es, ax
    xor di, di
    mov ax, 0xffff
    mov cx, 32000
    cld
    rep stosw
done:
    hlt
    jmp done

    times 510-($-$$) db 0
    dw 0xaa55

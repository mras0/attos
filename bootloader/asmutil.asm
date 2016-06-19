%define bochs_magic xchg bx, bx

%macro print_lit 1+
        jmp %%skip
        %%str db %1, 0
    %%skip:
        push si
        mov si, %%str
        call put_string
        pop si
%endmacro

put_crlf:
    push ax
    mov al, 13
    call put_char
    mov al, 10
    call put_char
    pop ax
    ret

put_space:
    push ax
    mov al, ' '
    call put_char
    pop ax
    ret

; al - number
put_hex8:
    push ax
    shr al, 4
    call put_digit
    pop ax
    call put_digit
    ret

; ax = number
put_hex16:
    push ax
    shr ax, 8
    call put_hex8
    pop ax
    call put_hex8
    ret

; si = addr to number
put_hex32:
    push ax
    mov ax, [si+2]
    call put_hex16
    mov ax, [si]
    call put_hex16
    pop ax
    ret

; si = addr to number
put_hex64:
    add si, 4
    call put_hex32
    sub si, 4
    call put_hex32
    ret

; lower nibble of al = digit to print
put_digit:
    pusha
    and al, 0xF
    cmp al, 10
    jb .nothex
    add al, 'A'-'0'-10
.nothex:
    add al, '0'
    call put_char
    popa
    ret

; al = character to print
put_char:
    pusha
    mov ah, 0x0E   ; AH = 0Eh teletype output, AL = character
    mov bx, 0x0007 ; BH = page number, BL = foreground color
    int 0x10
    popa
    ret

put_string:
    pusha
.inner:
    lodsb
    and al, al
    jz  .done
    call put_char
    jmp .inner
.done:
    popa
    ret

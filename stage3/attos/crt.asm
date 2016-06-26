    bits 64
    default rel

    section .text

    global memset

; rcx = void* dest
; rdx = int c
; r8  = size_t count
memset:
    push rsi
    push rdi
    mov rdi, rcx
    mov rcx, r8
    mov al, dl
    rep stosb
    pop rdi
    pop rsi
    ret

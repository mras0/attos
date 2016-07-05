    bits 64
    default rel

    section .text

    global memset
    global switch_to ; void switch_to(uint64_t cs, uint64_t rip, uint64_t ss, uint64_t rsp, uint64_t flags)

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


switch_to:
    ; rsp-0x20 <- rip
    ; rsp-0x18 <- cs
    ; rsp-0x10 <- flags
    ; rsp-0x08 <- ss
    ; rsp-0x00 <- rsp
    mov [rsp-0x20], rdx ; rip
    mov [rsp-0x18], rcx ; cs
    mov rcx, [rsp-0x08] ; rcx <- flags
    mov [rsp-0x10], rcx ; flags
    mov [rsp-0x08], r9  ; rsp
    mov [rsp-0x00], r8  ; ss (overwriting return address!)
    iretq

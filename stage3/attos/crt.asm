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
    ; rsp+0x00 <- rip
    mov [rsp+0x00], rdx ; rip (overwriting return address!)
    ; rsp+0x08 <- cs
    mov [rsp+0x08], rcx ; cs
    ; rsp+0x10 <- flags
    mov rcx, [rsp+0x28] ; rcx <- flags (parameter)
    mov [rsp+0x10], rcx ; flags
    ; rsp+0x18 <- rsp
    mov [rsp+0x18], r9  ; rsp
    ; rsp+0x20 <- ss
    mov [rsp+0x20], r8  ; ss
    iretq

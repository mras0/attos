    bits 64
    default rel

    section .text

    global memset
    global memcmp
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

; rcx = const void* p1
; rdx = const void* p2
; r8  = size_t      count
memcmp:
.inner:
    push rsi
    push rdi
    mov rdi, rcx
    mov rsi, rdx
    mov rcx, r8
    repe cmpsb
    mov al, 0
    je .done
    mov al, [rdi-1]
    sub al, [rsi-1]
.done:
    pop rdi
    pop rsi
    movsx eax, al
    ret

switch_to:
    mov rax, [rsp+0x28] ; rax <- flags (parameter)

    ; rsp+0x00 <- rip
    mov [rsp+0x00], rdx ; rip (overwriting return address!)
    ; rsp+0x08 <- cs
    mov [rsp+0x08], rcx ; cs
    ; rsp+0x10 <- flags
    mov [rsp+0x10], rax ; flags
    ; rsp+0x18 <- rsp
    mov [rsp+0x18], r9  ; rsp
    ; rsp+0x20 <- ss
    mov [rsp+0x20], r8  ; ss

    ;xchg bx, bx ;bochs_magic
    iretq

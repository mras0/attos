    bits 64
    default rel

%include "attos/pe.inc"

    section .text

    global memset
    global memcpy
    global memcmp
    global switch_to
    global switch_to_restore
    global bochs_magic

; rcx = void*  dest
; rdx = int    c
; r8  = size_t count
win64_proc memset
    win64_prologue_push rsi
    win64_prologue_push rdi
    win64_prologue_end
    mov rdi, rcx
    mov rcx, r8
    mov al, dl
    rep stosb
    win64_epilogue
    ret
win64_proc_end

; rcx = void*       dest
; rdx = const void* src
; r8  = size_t      count
win64_proc memcpy
    win64_prologue_push rsi
    win64_prologue_push rdi
    win64_prologue_end
    mov rdi, rcx
    mov rsi, rdx
    mov rcx, r8
    rep movsb
    win64_epilogue
    ret
win64_proc_end

; rcx = const void* p1
; rdx = const void* p2
; r8  = size_t      count
win64_proc memcmp
.inner:
    win64_prologue_push rsi
    win64_prologue_push rdi
    win64_prologue_end
    mov rdi, rcx
    mov rsi, rdx
    mov rcx, r8
    repe cmpsb
    mov al, 0
    je .done
    mov al, [rdi-1]
    sub al, [rsi-1]
.done:
    movsx eax, al
    win64_epilogue
    ret
win64_proc_end

; void switch_to(uint64_t cs, uint64_t rip, uint64_t ss, uint64_t rsp, uint64_t flags, uint64_t& tss_rsp0)
switch_to:
; On entry:
;
; rsp + 0x30  tss_rsp0
; rsp + 0x28  flags
; rsp + 0x20  shadow space
; rsp + 0x18  shadow space
; rsp + 0x10  shadow space
; rsp + 0x08  shadow space
; rsp + 0x00  return address

    mov rax, [rsp+0x28] ; rax <- flags (parameter)
    mov r10, [rsp+0x30] ; r10 <- tss_rsp0

    ; Save nonvolatile reigsters
    pushfq
    push r12
    push r13
    push r14
    push r15
    push rdi
    push rsi
    push rbx
    push rbp
    ; TODO: Save XMM6-XMM15
    mov r11, rsp
    mov [switch_to_orig_rsp], r11

    mov [r10], rsp

    ; Make room for IRETQ frame
    sub rsp, 0x28

    ; rsp+0x00 <- rip
    mov [rsp+0x00], rdx ; rip
    ; rsp+0x08 <- cs
    mov [rsp+0x08], rcx ; cs
    ; rsp+0x10 <- flags
    mov [rsp+0x10], rax ; flags
    ; rsp+0x18 <- rsp
    mov [rsp+0x18], r9  ; rsp
    ; rsp+0x20 <- ss
    mov [rsp+0x20], r8  ; ss

; Before IRETQ
;
; rsp + 0x20  ss
; rsp + 0x18  rsp
; rsp + 0x10  flags
; rsp + 0x08  cs
; rsp + 0x00  rip

    iretq

; void switch_to_restore()
switch_to_restore:
    mov rsp, [switch_to_orig_rsp]
    pop rbp
    pop rbx
    pop rsi
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    popfq
    ret

bochs_magic:
    xchg bx, bx
    ret

    section .bss
switch_to_orig_rsp resq 1

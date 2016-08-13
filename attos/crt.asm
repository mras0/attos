    bits 64
    default rel

%include "attos/pe.inc"

    section .text

    global memset
    global memcpy
    global memcmp
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

bochs_magic:
    xchg bx, bx
    ret

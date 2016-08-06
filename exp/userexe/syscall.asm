    bits 64
    default rel

%include "attos/pe.inc"

    section .text

    global syscall1
    global syscall2

syscall2:
win64_proc syscall1
    win64_prologue_push r11
    win64_prologue_end
    mov rax, rcx
    syscall
    win64_epilogue
    ret
win64_proc_end

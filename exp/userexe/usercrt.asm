    bits 64
    default rel

%include "attos/pe.inc"

    section .text

    global mainCRTStartup
    global syscall1
    extern main

win64_proc mainCRTStartup
    win64_prologue_alloc 0x28
    win64_prologue_end
    call main
    win64_epilogue
    xor rax, rax
    mov edx, 0xFEDE0ABE
    syscall
win64_proc_end

win64_proc syscall1
    win64_prologue_push r11
    win64_prologue_end
    mov rax, rcx
    syscall
    win64_epilogue
    ret
win64_proc_end

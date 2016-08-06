    bits 64
    default rel

%include "attos/pe.inc"

    section .text

    global mainCRTStartup
    extern main

win64_proc mainCRTStartup
    win64_prologue_alloc 0x28
    win64_prologue_end
    call main
    win64_epilogue
    xchg bx, bx
    int 0x80
win64_proc_end

    section .text


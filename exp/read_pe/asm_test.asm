    bits 64
    default rel

%include "../../stage3/attos/pe.inc"

    [section .text]

    global test_fun
    extern foo

win64_proc test_fun
    win64_prologue_push RBP
    mov rbp, rsp
    win64_prologue_alloc 0x20
    win64_prologue_end
    call foo
    ;add rsp, 0x20
    ;pop rbp
    win64_epilogue
    ret
win64_proc_end

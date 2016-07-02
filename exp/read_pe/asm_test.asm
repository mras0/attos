    bits 64
    default rel

%include "../../stage3/attos/pe.inc"

struc registers
    .rax             resq 1
    .rbx             resq 1
    .rcx             resq 1
    .rdx             resq 1
    .rbp             resq 1
    .rsi             resq 1
    .rdi             resq 1
    .r8              resq 1
    .r9              resq 1
    .r10             resq 1
    .r11             resq 1
    .r12             resq 1
    .r13             resq 1
    .r14             resq 1
    .r15             resq 1
    .fx_state        resb 512
    .reserved        resq 1 ; For alignment
registers_saved_size: ; only the above are saved by code
    .interrupt_no    resq 1 ; pushed by isr
    .error_code      resq 1 ; pushed by system (or isr)
    .rip             resq 1 ; system stack
    .cs              resq 1 ; ..
    .rflags          resq 1
    .rsp             resq 1
    .ss              resq 1
endstruc

    [section .text]

    global test_fun
    extern foo

%macro save_reg 1
    ;mov [rsp+registers.%1], %1
    win64_prologue_save_reg registers.%+%1, %1
%endmacro

win64_proc test_fun
    win64_prologue_push RBP
    mov rbp, rsp
    win64_prologue_alloc registers_saved_size
    save_reg rax
    save_reg rbx
    save_reg rcx
    save_reg rdx
    save_reg rbp
    save_reg rsi
    save_reg rdi
    save_reg r8
    save_reg r9
    save_reg r10
    save_reg r11
    save_reg r12
    save_reg r13
    save_reg r14
    save_reg r15
    win64_prologue_end
    call foo
    win64_epilogue
    ret
win64_proc_end


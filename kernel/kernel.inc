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
    .reserved2       resq 1 ; For alignment of fx state
registers_saved_size: ; only the above are saved by code
    .interrupt_no    resq 1 ; pushed by isr
    .error_code      resq 1 ; pushed by system (or isr)
    .rip             resq 1 ; system stack
    .cs              resq 1 ; ..
    .rflags          resq 1
    .rsp             resq 1
    .ss              resq 1
endstruc

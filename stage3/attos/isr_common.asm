    bits 64
    default rel

%include "attos/pe.inc"

    section .text

    global isr_common
    extern interrupt_service_routine ; void interrupt_service_routine(registers&);

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

struc interrupt_gate
    .offset_low  resw 1
    .selector    resw 1
    .ist         resb 1 ; 0
    .type        resb 1 ; lsb->msb 4bits type, 0, 2bits dpl, 1 bit present
    .offset_mid  resw 1
    .offset_high resd 1
    .reserved    resd 1
endstruc

%define win64_shadow_space_size 32
%define isr_common_stack_alloc registers_saved_size + win64_shadow_space_size

%define isr_common_reg_offset(REG) win64_shadow_space_size + registers.%+REG

%macro save_reg 1
    ;mov [rsp + isr_common_reg_offset(%1)], %1
    win64_prologue_save_reg isr_common_reg_offset(%1), %1
%endmacro

    align 16
win64_proc isr_common
    win64_prologue_alloc_unwind 2*8 ; error_code and interrupt_no
    ; save registers
    win64_prologue_alloc isr_common_stack_alloc
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

    ; save fx state
    fxsave [rsp+isr_common_reg_offset(fx_state)]

    ; ensure direction flag is cleared
    cld

    lea  rcx, [rsp+win64_shadow_space_size] ; arg = registers*
    call interrupt_service_routine

    ; restore fx state
    fxrstor [rsp+isr_common_reg_offset(fx_state)]

    ; restore registers
    win64_epilogue
    iretq

win64_prologue_end

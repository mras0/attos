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
;%define isr_common_reg_offset rsp + win64_shadow_space_size
%define isr_common_stack_alloc registers_saved_size + win64_shadow_space_size
%define isr_common_stack_adjust isr_common_stack_alloc + 2 * 8 ; error_code and interrupt_no

%define isr_common_reg_offset(REG) win64_shadow_space_size + registers.%+REG

%macro save_reg 1
    mov [rsp + isr_common_reg_offset(%1)], %1
%endmacro

%macro restore_reg 1
    mov %1, [rsp + isr_common_reg_offset(%1)]
%endmacro

    align 16
isr_common:
    ; save registers
    sub rsp, isr_common_stack_alloc
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

    ; save fx state
    fxsave [rsp+isr_common_reg_offset(fx_state)]

    cld                  ; ensure direction flag is cleared
    lea  rcx, [rsp+win64_shadow_space_size]         ; arg = registers*
    call interrupt_service_routine

    ; restore fx state
    fxrstor [rsp+isr_common_reg_offset(fx_state)]

    ; restore registers
    restore_reg rax
    restore_reg rbx
    restore_reg rcx
    restore_reg rdx
    restore_reg rbp
    restore_reg rsi
    restore_reg rdi
    restore_reg r8
    restore_reg r9
    restore_reg r10
    restore_reg r11
    restore_reg r12
    restore_reg r13
    restore_reg r14
    restore_reg r15
    add         rsp, isr_common_stack_adjust

    iretq

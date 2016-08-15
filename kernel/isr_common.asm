    bits 64
    default rel

%include "attos/pe.inc"
%include "kernel.inc"

    section .text

    global isr_common

    extern interrupt_service_routine ; void interrupt_service_routine(registers&)

struc interrupt_gate
    .offset_low  resw 1
    .selector    resw 1
    .ist         resb 1 ; 0
    .type        resb 1 ; lsb->msb 4bits type, 0, 2bits dpl, 1 bit present
    .offset_mid  resw 1
    .offset_high resd 1
    .reserved    resd 1
endstruc

%define isr_local_size (32) ; 32 bytes for the shadow space
%define isr_common_stack_alloc registers.saved_size + isr_local_size
%define isr_registers_offset isr_local_size

%define isr_common_reg_offset(REG)  isr_registers_offset + registers.%+REG

%macro isr_save_reg 1
    ;mov [rsp + isr_common_reg_offset(%1)], %1
    win64_prologue_save_reg isr_common_reg_offset(%1), %1
%endmacro

    align 16
win64_proc isr_common
    win64_prologue_push_machineframe_unwind
    win64_prologue_alloc_unwind 2*8 ; error_code and interrupt_no
    ; save registers
    win64_prologue_alloc isr_common_stack_alloc
    isr_save_reg rax
    isr_save_reg rbx
    isr_save_reg rcx
    isr_save_reg rdx
    isr_save_reg rbp
    isr_save_reg rsi
    isr_save_reg rdi
    isr_save_reg r8
    isr_save_reg r9
    isr_save_reg r10
    isr_save_reg r11
    isr_save_reg r12
    isr_save_reg r13
    isr_save_reg r14
    isr_save_reg r15
    win64_prologue_end

    ; save fx state
    fxsave [rsp+isr_common_reg_offset(fx_state)]

    ; ensure direction flag is cleared
    cld

    lea  rcx, [rsp+isr_registers_offset] ; arg = registers&
    call interrupt_service_routine

    ; restore fx state
    fxrstor [rsp+isr_common_reg_offset(fx_state)]

    ; restore registers
    win64_epilogue

    iretq

win64_proc_end

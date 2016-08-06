    bits 64
    default rel

%include "attos/pe.inc"

    section .text

    global isr_common
    global syscall_handler

    extern interrupt_service_routine ; void interrupt_service_routine(registers&)
    extern syscall_service_routine   ; void syscall_service_routine(registers&)

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

struc interrupt_gate
    .offset_low  resw 1
    .selector    resw 1
    .ist         resb 1 ; 0
    .type        resb 1 ; lsb->msb 4bits type, 0, 2bits dpl, 1 bit present
    .offset_mid  resw 1
    .offset_high resd 1
    .reserved    resd 1
endstruc

%define isr_local_size (32+8) ; 32 bytes for the shadow space and 8 bytes to ensure alignment
%define isr_common_stack_alloc registers_saved_size + isr_local_size
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

win64_proc_end

%define syscall_local_size (32+8) ; 32 bytes for the shadow space + 8 for alignment
%define syscall_common_stack_alloc registers_size + syscall_local_size
%define syscall_registers_offset syscall_local_size
%define syscall_common_reg_offset(REG)  syscall_registers_offset + registers.%+REG

%macro syscall_save_reg 1
    win64_prologue_save_reg syscall_common_reg_offset(%1), %1
%endmacro

    align 16
win64_proc syscall_handler
    ; switch to kernel stack
    xchg [syscall_stack_ptr], rsp
    ; save registers
    win64_prologue_alloc registers_saved_size
    syscall_save_reg rax
    syscall_save_reg rbx
    syscall_save_reg rcx
    syscall_save_reg rdx
    syscall_save_reg rbp
    syscall_save_reg rsi
    syscall_save_reg rdi
    syscall_save_reg r8
    syscall_save_reg r9
    syscall_save_reg r10
    syscall_save_reg r11
    syscall_save_reg r12
    syscall_save_reg r13
    syscall_save_reg r14
    syscall_save_reg r15
    win64_prologue_end

    ; save fx state
    fxsave [rsp+syscall_common_reg_offset(fx_state)]

    lea  rcx, [rsp+syscall_registers_offset] ; arg = registers&
    call syscall_service_routine

    ; restore fx state
    fxrstor [rsp+syscall_common_reg_offset(fx_state)]

    ; restore registers
    win64_epilogue
    ; restore user stack
    xchg [syscall_stack_ptr], rsp

    ; Force REX.W prefix to ensure 64-bit return
    o64 sysret

win64_proc_end

    section .data
    syscall_stack_ptr dq syscall_kernel_stack_top

    section .bss
    align 4096
    resb 4096
syscall_kernel_stack_top:

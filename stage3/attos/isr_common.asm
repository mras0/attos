    bits 64
    default rel

    %define KERNEL_CS 0x28 ; Matching ../bootloader/stage2.asm

    section .text

    global isr_common
    extern interrupt_service_routine ; void interrupt_service_routine(registers*);

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

%macro save_reg 1
    mov [rsp+registers.%1], %1
%endmacro

%macro restore_reg 1
    mov %1, [rsp+registers.%1]
%endmacro

    align 16
isr_common:
    ; save registers
    sub rsp, registers_saved_size
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

    cld ; ensure direction flag is cleared

    mov  rbp, rsp  ; save stack pointer
    and  rsp, -16  ; align stack
    sub  rsp, 0x20 ; make room for the function to preserve rcx, rdx, r8 and r9
    mov  rcx, rbp  ; arg = registers*
    call interrupt_service_routine
    mov  rsp, rbp  ; restore stack pointer

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
    add         rsp, registers_saved_size + 2 * 8 ; error_code and interrupt_no

    iretq

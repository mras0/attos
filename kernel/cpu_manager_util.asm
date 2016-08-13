    bits 64
    default rel

    section .text

%include "attos/pe.inc"
%include "kernel.inc"

    global switch_to
    global switch_to_restore
    global syscall_handler

    extern syscall_service_routine   ; void syscall_service_routine(registers&)

%macro restore_registers_rcx_last 1
    mov rax, [%1 + registers.rax]
    mov rbx, [%1 + registers.rbx]  ; volatile
    mov rdx, [%1 + registers.rdx]
    mov rbp, [%1 + registers.rbp]  ; volatile
    mov rsi, [%1 + registers.rsi]  ; volatile
    mov rdi, [%1 + registers.rdi]  ; volatile
    mov r8,  [%1 + registers.r8]
    mov r9,  [%1 + registers.r9]
    mov r10, [%1 + registers.r10]
    mov r11, [%1 + registers.r11]
    mov r12, [%1 + registers.r12]  ; volatile
    mov r13, [%1 + registers.r13]  ; volatile
    mov r14, [%1 + registers.r14]  ; volatile
    mov r15, [%1 + registers.r15]  ; volatile
    fxrstor [%1 + registers.fx_state] ; xmm6-xmm15 volatile
    mov rcx, [%1 + registers.rcx]
%endmacro

switch_to_stack_adjust equ registers_size + 8

; void switch_to(registers& regs, uint64_t& saved_rsp)
win64_proc switch_to
; On entry:
;
; rsp + 0x20  shadow space
; rsp + 0x18  shadow space
; rsp + 0x10  shadow space
; rsp + 0x08  shadow space
; rsp + 0x00  return address

    ; Save nonvolatile reigsters
    win64_prologue_alloc switch_to_stack_adjust

    mov [rsp + registers.rax], rax
    mov [rsp + registers.rbx], rbx ; volatile
    mov [rsp + registers.rcx], rcx
    mov [rsp + registers.rdx], rdx
    mov [rsp + registers.rbp], rbp ; volatile
    mov [rsp + registers.rsi], rsi ; volatile
    mov [rsp + registers.rdi], rdi ; volatile
    mov [rsp + registers.r8],  r8
    mov [rsp + registers.r9],  r9
    mov [rsp + registers.r10], r10
    mov [rsp + registers.r11], r11
    mov [rsp + registers.r12], r12 ; volatile
    mov [rsp + registers.r13], r13 ; volatile
    mov [rsp + registers.r14], r14 ; volatile
    mov [rsp + registers.r15], r15 ; volatile
    ;TODO:.rip
    mov [rsp + registers.cs], cs
    pushfq
    pop qword [rsp + registers.rflags]
    mov [rsp + registers.rsp], rsp
    mov [rsp + registers.cs], ss

    fxsave [rsp + registers.fx_state] ; xmm6-xmm15 volatile

    mov [rdx], rsp

    ; Make room for IRETQ frame
    win64_prologue_alloc 0x28
    win64_prologue_end

    ; rsp+0x00 <- rip
    mov rax, [rcx + registers.rip]
    mov [rsp+0x00], rax
    ; rsp+0x08 <- cs
    mov rax, [rcx+registers.cs]
    mov [rsp+0x08], rax
    ; rsp+0x10 <- flags
    mov rax, [rcx+registers.rflags]
    mov [rsp+0x10], rax
    ; rsp+0x18 <- rsp
    mov rax, [rcx+registers.rsp]
    mov [rsp+0x18], rax
    ; rsp+0x20 <- ss
    mov rax, [rcx+registers.ss]
    mov [rsp+0x20], rax

    restore_registers_rcx_last rcx

    ; Before IRETQ
    ;
    ; rsp + 0x20  ss
    ; rsp + 0x18  rsp
    ; rsp + 0x10  flags
    ; rsp + 0x08  cs
    ; rsp + 0x00  rip

    iretq
win64_proc_end

; void switch_to_restore(uint64_t& saved_rsp)
win64_proc switch_to_restore
    win64_prologue_alloc_unwind switch_to_stack_adjust
    win64_prologue_end
    mov rsp, [rcx]
    restore_registers_rcx_last rsp
    push qword [rsp + registers.rflags]
    popfq
    add rsp, switch_to_stack_adjust
    ret
win64_proc_end

%define syscall_local_size (32) ; 32 bytes for the shadow space
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
    win64_prologue_alloc syscall_common_stack_alloc
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

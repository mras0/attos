    bits 64
    default rel

%include "attos/pe.inc"
%include "kernel.inc"

    section .text

    global switch_to
    global switch_to_restore
    global syscall_handler

    extern syscall_service_routine   ; void syscall_service_routine(registers&)


; void switch_to(uint64_t cs, uint64_t rip, uint64_t ss, uint64_t rsp, uint64_t flags, uint64_t& saved_rsp)
switch_to:
; On entry:
;
; rsp + 0x30  saved_rsp
; rsp + 0x28  flags
; rsp + 0x20  shadow space
; rsp + 0x18  shadow space
; rsp + 0x10  shadow space
; rsp + 0x08  shadow space
; rsp + 0x00  return address

    mov rax, [rsp+0x28] ; rax <- flags (parameter)
    mov r10, [rsp+0x30] ; r10 <- saved_rsp

    ; Save nonvolatile reigsters
    pushfq
    push r12
    push r13
    push r14
    push r15
    push rdi
    push rsi
    push rbx
    push rbp
    ; TODO: Save XMM6-XMM15
    mov [r10], rsp

    ; Make room for IRETQ frame
    sub rsp, 0x28

    ; rsp+0x00 <- rip
    mov [rsp+0x00], rdx ; rip
    ; rsp+0x08 <- cs
    mov [rsp+0x08], rcx ; cs
    ; rsp+0x10 <- flags
    mov [rsp+0x10], rax ; flags
    ; rsp+0x18 <- rsp
    mov [rsp+0x18], r9  ; rsp
    ; rsp+0x20 <- ss
    mov [rsp+0x20], r8  ; ss

; Before IRETQ
;
; rsp + 0x20  ss
; rsp + 0x18  rsp
; rsp + 0x10  flags
; rsp + 0x08  cs
; rsp + 0x00  rip

    iretq

; void switch_to_restore(uint64_t& saved_rsp)
switch_to_restore:
    mov rsp, [rcx]
    pop rbp
    pop rbx
    pop rsi
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    popfq
    ret

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

    bits 64

    mov ax, cs
    mov ax, ds
    mov ax, es
    mov ds, ax
    mov es, ax

    mov rax, rsp
    push 0x10     ; ss
    add rax, 8
    push rax      ; rsp
    add rax, -8
    push 0x2      ; eflags (bit 1 is always set)
    push 0x08     ; cs
    push qword [rax] ; rip
    iretq

    nop
    push rcx
    push rdx
    push r8
    push r9

    push rbp
    mov rbp, rsp
    mov rsp, rcx

    nop

    mov rsp, rbp
    pop rbp

    nop


    mov al, 12
    mov ax, 12
    mov eax, 12
    mov rax, 0xABCDABCDABCDABCD

    mov r8b, 12
    mov r8w, 12
    mov r8d, 12
    mov r8, 0xABCDABCDABCDABCD

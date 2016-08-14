    bits 64
    default rel

    section .text

    global main

main:
    mov eax, 0 ; syscall-number::exit
    mov edx, 42
    syscall

    bits 64
    default rel

    global test_fun
    extern foo

%define UWOP_PUSH_NONVOL       0; 1 node.    Push a nonvolatile integer register, decrementing RSP by 8. The operation info is the number of the register.
%define UWOP_ALLOC_LARGE       1; 2/3 nodes. Allocate a large-sized area on the stack.
%define UWOP_ALLOC_SMALL       2; 1 node.    Allocate a small-sized area on the stack.
%define UWOP_SET_FPREG         3; 1 node.    Establish the frame pointer register by setting the register to some offset of the current RSP.
%define UWOP_SAVE_NONVOL       4; 2 nodes.   Save a nonvolatile integer register on the stack using a MOV instead of a PUSH.
%define UWOP_SAVE_NONVOL_FAR   5; 3 nodes.   Save a nonvolatile integer register on the stack with a long offset, using a MOV instead of a PUSH.
%define UWOP_SAVE_XMM128       8; 2 nodes.   Save all 128 bits of a nonvolatile XMM register on the stack.
%define UWOP_SAVE_XMM128_FAR   9; 3 nodes.   Save all 128 bits of a nonvolatile XMM register on the stack with a long offset.
%define UWOP_PUSH_MACHFRAME   10; 1 node.    Push a machine frame.

%define UWOP_REG_RAX    0
%define UWOP_REG_RCX    1
%define UWOP_REG_RDX    2
%define UWOP_REG_RBX    3
%define UWOP_REG_RSP    4
%define UWOP_REG_RBP    5
%define UWOP_REG_RSI    6
%define UWOP_REG_RDI    7
%define UWOP_REG_R8     8
%define UWOP_REG_R9     9
%define UWOP_REG_R10   10
%define UWOP_REG_R11   11
%define UWOP_REG_R12   12
%define UWOP_REG_R13   13
%define UWOP_REG_R14   14
%define UWOP_REG_R15   15

; struct UNWIND_CODE {
;    uint8_t CodeOffset; // Offset in prolog
;    uint8_t UnwindOp:4; // Unwind operation code
;    uint8_t OpInfo:4;   // Operation information
; };

    [section .text]

%xdefine unwind_codes

test_fun:
lab0:
%xdefine unwind_codes (lab0-test_fun), UWOP_PUSH_NONVOL | (UWOP_REG_RBP << 4), unwind_codes
    push rbp
    mov rbp, rsp
lab1:
%xdefine unwind_codes (lab1-test_fun), UWOP_ALLOC_SMALL | (((0x28 / 8)-1) << 4), unwind_codes
    sub rsp, 0x28
test_fun_prolog_end:
    call foo
    add rsp, 0x28
    pop rbp
    ret
test_fun_end:

    [section .pdata rdata align=4]
    ; RUNTIME_FUNCTION
    dd test_fun wrt ..imagebase
    dd test_fun_end wrt ..imagebase
    dd test_fun_unwind wrt ..imagebase

    [section .xdata rdata align=8]
test_fun_unwind:
    db 1 | (0 << 3)                                          ; Version = 1, Flags = 0
    db test_fun_prolog_end-test_fun                          ; SizeOfProlog
    db (test_fun_unwind_codes_end-test_fun_unwind_codes) / 2 ; CountOfCodes
    db 0 | (0 << 4)                                          ; FrameRegister=0, FrameOffset=0
test_fun_unwind_codes:
    db unwind_codes
    align 4
test_fun_unwind_codes_end:

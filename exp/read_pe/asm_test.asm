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

    [section .text]


%macro win64_proc 1
%xdefine win64_proc_name %1
%xdefine win64_unwind_codes
%1:
%endmacro

%macro win64_prologue_push 1
%%here:
%xdefine win64_unwind_codes (%%here-win64_proc_name), UWOP_PUSH_NONVOL | (UWOP_REG_ %+ %1 << 4), win64_unwind_codes
    push %1
%endmacro

%macro win64_prologue_alloc 1
%if %1 <= 128
%%here:
%xdefine win64_unwind_codes (%%here-win64_proc_name), UWOP_ALLOC_SMALL | (((%1 / 8)-1) << 4), win64_unwind_codes
    sub rsp, %1
%else
%error "Not implemented"
%endif
%endmacro

%macro win64_prologue_end 0
%%here:
%define win64_proc_prologue_end %%here
%endmacro

win64_proc test_fun
    win64_prologue_push RBP
    mov rbp, rsp
    win64_prologue_alloc 0x20
    win64_prologue_end
    call foo
    add rsp, 0x20
    pop rbp
    ret
%macro win64_proc_end 0
%%end:

    [section .pdata rdata align=4]
    ; RUNTIME_FUNCTION
    dd win64_proc_name wrt ..imagebase
    dd %%end wrt ..imagebase
    dd %%unwind wrt ..imagebase

    [section .xdata rdata align=8]
%%unwind:
    db 1 | (0 << 3)                                          ; Version = 1, Flags = 0
    db win64_proc_prologue_end-win64_proc_name               ; SizeOfProlog
    db (%%codes_end-%%codes) / 2 ; CountOfCodes
    db 0 | (0 << 4)                                          ; FrameRegister=0, FrameOffset=0
%%codes:
    db win64_unwind_codes
%%codes_end:
    align 8
    __SECT__

%undef win64_proc_prologue_end
%undef win64_unwind_codes
%undef win64_proc_name
%endmacro
    win64_proc_end

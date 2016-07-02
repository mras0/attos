struc IMAGE_DOS_HEADER
    .e_magic    resw 1
    .e_cblp     resw 1
    .e_cp       resw 1
    .e_crlc     resw 1
    .e_cparhdr  resw 1
    .e_minalloc resw 1
    .e_maxalloc resw 1
    .e_ss       resw 1
    .e_sp       resw 1
    .e_csum     resw 1
    .e_ip       resw 1
    .e_cs       resw 1
    .e_lfarlc   resw 1
    .e_ovno     resw 1
    .e_res      resw 4
    .e_oemid    resw 1
    .e_oeminfo  resw 1
    .e_res2     resw 10
    .e_lfanew   resd 1
endstruc
IMAGE_DOS_SIGNATURE equ 0x5A4D     ; MZ

struc IMAGE_FILE_HEADER
  .Machine              resw 1
  .NumberOfSections     resw 1
  .TimeDateStamp        resd 1
  .PointerToSymbolTable resd 1
  .NumberOfSymbols      resd 1
  .SizeOfOptionalHeader resw 1
  .Characteristics      resw 1
endstruc

IMAGE_FILE_MACHINE_AMD64 EQU 0x8664

struc IMAGE_OPTIONAL_HEADER32
    .Magic                       resw 1
    .MajorLinkerVersion          resb 1
    .MinorLinkerVersion          resb 1
    .SizeOfCode                  resd 1
    .SizeOfInitializedData       resd 1
    .SizeOfUninitializedData     resd 1
    .AddressOfEntryPoint         resd 1
    .BaseOfCode                  resd 1
    .BaseOfData                  resd 1
    .ImageBase                   resd 1
    .SectionAlignment            resd 1
    .FileAlignment               resd 1
    .MajorOperatingSystemVersion resw 1
    .MinorOperatingSystemVersion resw 1
    .MajorImageVersion           resw 1
    .MinorImageVersion           resw 1
    .MajorSubsystemVersion       resw 1
    .MinorSubsystemVersion       resw 1
    .Win32VersionValue           resd 1
    .SizeOfImage                 resd 1
    .SizeOfHeaders               resd 1
    .CheckSum                    resd 1
    .Subsystem                   resw 1
    .DllCharacteristics          resw 1
    .SizeOfStackReserve          resd 1
    .SizeOfStackCommit           resd 1
    .SizeOfHeapReserve           resd 1
    .SizeOfHeapCommit            resd 1
    .LoaderFlags                 resd 1
    .NumberOfRvaAndSizes         resd 1
    ;IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]
endstruc

struc IMAGE_OPTIONAL_HEADER64
    .Magic                       resw 1
    .MajorLinkerVersion          resb 1
    .MinorLinkerVersion          resb 1
    .SizeOfCode                  resd 1
    .SizeOfInitializedData       resd 1
    .SizeOfUninitializedData     resd 1
    .AddressOfEntryPoint         resd 1
    .BaseOfCode                  resd 1
    .ImageBase                   resq 1
    .SectionAlignment            resd 1
    .FileAlignment               resd 1
    .MajorOperatingSystemVersion resw 1
    .MinorOperatingSystemVersion resw 1
    .MajorImageVersion           resw 1
    .MinorImageVersion           resw 1
    .MajorSubsystemVersion       resw 1
    .MinorSubsystemVersion       resw 1
    .Win32VersionValue           resd 1
    .SizeOfImage                 resd 1
    .SizeOfHeaders               resd 1
    .CheckSum                    resd 1
    .Subsystem                   resw 1
    .DllCharacteristics          resw 1
    .SizeOfStackReserve          resq 1
    .SizeOfStackCommit           resq 1
    .SizeOfHeapReserve           resq 1
    .SizeOfHeapCommit            resq 1
    .LoaderFlags                 resd 1
    .NumberOfRvaAndSizes         resd 1
    ;IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]
endstruc

struc IMAGE_NT_HEADERS
    .Signature      resd 1
    .FileHeader     resb IMAGE_FILE_HEADER_size
    .OptionalHeader resb IMAGE_OPTIONAL_HEADER64_size
endstruc

IMAGE_NT_SIGNATURE   equ 0x00004550 ; PE00
SIZE_OF_NT_SIGNATURE equ 4

IMAGE_SIZEOF_SHORT_NAME equ 8

struc IMAGE_SECTION_HEADER
  .Name                 resb IMAGE_SIZEOF_SHORT_NAME
  .PhysicalAddress:
  .VirtualSize:
  .Misc                 resd 1
  .VirtualAddress       resd 1
  .SizeOfRawData        resd 1
  .PointerToRawData     resd 1
  .PointerToRelocations resd 1
  .PointerToLinenumbers resd 1
  .NumberOfRelocations  resw 1
  .NumberOfLinenumbers  resw 1
  .Characteristics      resd 1
endstruc

IMAGE_SCN_CNT_CODE                  equ 0x00000020 ; The section contains executable code.
IMAGE_SCN_CNT_INITIALIZED_DATA      equ 0x00000040 ; The section contains initialized data.
IMAGE_SCN_CNT_UNINITIALIZED_DATA    equ 0x00000080 ; The section contains uninitialized data.
IMAGE_SCN_MEM_EXECUTE               equ 0x20000000 ; The section can be executed as code.
IMAGE_SCN_MEM_READ                  equ 0x40000000 ; The section can be read.
IMAGE_SCN_MEM_WRITE                 equ 0x80000000 ; The section can be written to.


;
; Unwind information
;

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

; Inspired by https://gist.github.com/rygorous/f729919ff64526a46e591d8f8b52058e

%macro win64_proc 1
    %xdefine win64_proc_name %1
    %xdefine win64_unwind_codes
%1:
%endmacro

%macro win64_prologue_push 1
%%here:
    %xdefine win64_unwind_codes (%%here-win64_proc_name), UWOP_PUSH_NONVOL | (UWOP_REG_%+%1 << 4), win64_unwind_codes
    push %1
%endmacro

%macro win64_prologue_alloc 1
%%here:
    %if %1 <= 128 ; 8 to 128 bytes
        %xdefine win64_unwind_codes (%%here-win64_proc_name), UWOP_ALLOC_SMALL | (((%1 / 8)-1) << 4), win64_unwind_codes
        sub rsp, %1
    %elif %1 <= 512<<10 ; 136 to 512K-8 bytes
        %xdefine win64_unwind_codes (%%here-win64_proc_name), UWOP_ALLOC_LARGE | 0, (%1 >> 3) & 0xff, (%1 >> 11), win64_unwind_codes
        sub rsp, %1
    %else
        %error "Not implemented"
    %endif
%endmacro

%macro win64_prologue_end 0
%%here:
    %define win64_proc_prologue_end %%here
%endmacro

%macro win64_pop_nonvol 1
    %if %1 == UWOP_REG_RAX
        pop rax
    %elif %1 == UWOP_REG_RCX
        pop rcx
    %elif %1 == UWOP_REG_RDX
        pop rdx
    %elif %1 == UWOP_REG_RBX
        pop rbx
    %elif %1 == UWOP_REG_RSP
        pop rsp
    %elif %1 == UWOP_REG_RBP
        pop rbp
    %elif %1 == UWOP_REG_RSI
        pop rsi
    %elif %1 == UWOP_REG_RDI
        pop rdi
    %elif %1 == UWOP_REG_R8
        pop r8
    %elif %1 == UWOP_REG_R9
        pop r9
    %elif %1 == UWOP_REG_R10
        pop r10
    %elif %1 == UWOP_REG_R11
        pop r11
    %elif %1 == UWOP_REG_R12
        pop r12
    %elif %1 == UWOP_REG_R13
        pop r13
    %elif %1 == UWOP_REG_R14
        pop r14
    %elif %1 == UWOP_REG_R15
        pop r15
    %else
        %error %1 is invalid
    %endif
%endmacro

%macro win64_restore 2-*
    %if %0 % 2 == 1
        %error %0 is odd
    %endif
    %assign cnt 0
    %rep 100
        %rotate 1
        %if ((%1)&15) == UWOP_PUSH_NONVOL
            win64_pop_nonvol (%1>>4)
        %elif ((%1)&15) == UWOP_ALLOC_SMALL
            add rsp, ((%1 >> 4) + 1) * 8
        %elif ((%1)&15) == UWOP_ALLOC_LARGE
            %if (%1)>>4 != 0
                %error really large stack alloc
            %endif
            %assign cnt cnt+2
            add rsp, (%2 + (%3 * 256)) * 8
            %rotate 2
        %else
            %error "Unimplmented OP"
        %endif
        %rotate 1
        %assign cnt cnt+2
        %if cnt == %0
            %exitrep
        %elif cnt > %0
            %error too many arguments popped
        %endif
    %endrep
%endmacro

%macro win64_epilogue 0
    win64_restore win64_unwind_codes
%endmacro

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


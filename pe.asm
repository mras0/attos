%include "pe.inc"

%macro pe_check_u16 2
    cmp word [%1], %2
    je %%ok
%defstr %%addr %1
%defstr %%expected %2
    print_lit 'pe_check_failed ', %%addr, ' != ', %%expected, 13, 10
    push ax
    mov ax, [%1]
    call put_hex16
    call put_crlf
    pop ax
    push ax
    mov ax, %2
    call put_hex16
    call put_crlf
    pop ax
    jmp exit
%%ok:
%endmacro

%macro pe_print 2
    push si
%defstr %%offstr %1
    print_lit %%offstr, ' '
    add si, %1
    call %2
    call put_crlf
    pop si
%endmacro

pe_put_hex16:
    push ax
    mov ax, [si]
    call put_hex16
    pop ax
    ret

%macro pe_print16 1
    pe_print %1, pe_put_hex16
%endmacro

%macro pe_print32 1
    pe_print %1, put_hex32
%endmacro

%macro pe_print64 1
    pe_print %1, put_hex64
%endmacro

pe_test:
    print_lit 'pe_test running', 13, 10

    ; IMAGE_DOS_HEADER
    mov si, stage3
    pe_check_u16 si+IMAGE_DOS_HEADER.e_magic, IMAGE_DOS_SIGNATURE
    add si, [si+IMAGE_DOS_HEADER.e_lfanew]

    ; IMAGE_NT_HEADERS
    pe_check_u16 si, IMAGE_NT_SIGNATURE & 0xffff
    pe_check_u16 si+2, IMAGE_NT_SIGNATURE>>16
    add si, SIZE_OF_NT_SIGNATURE

    ; IMAGE_FILE_HEADER
    pe_check_u16 si+IMAGE_FILE_HEADER.Machine, IMAGE_FILE_MACHINE_AMD64
    ; bx = SizeOfOptionalHeader
    mov bx, [si+IMAGE_FILE_HEADER.SizeOfOptionalHeader]
    ; cx = NumberOfSections
    mov cx, [si+IMAGE_FILE_HEADER.NumberOfSections]
    add si, IMAGE_FILE_HEADER_size

    ; IMAGE_OPTIONAL_HEADER
    pe_print16 IMAGE_OPTIONAL_HEADER64.Magic
    pe_print32 IMAGE_OPTIONAL_HEADER64.AddressOfEntryPoint
    pe_print64 IMAGE_OPTIONAL_HEADER64.ImageBase
    add si, bx

    ; IMAGE_SECTION_HEADER
    mov dx, 0
.loop:
    cmp dx, cx
    jge .done

    ; Print Name
    xor al, al
    xchg al, [si+IMAGE_SIZEOF_SHORT_NAME]
    call put_string
    xchg al, [si+IMAGE_SIZEOF_SHORT_NAME]
    call put_crlf

    pe_print32 IMAGE_SECTION_HEADER.VirtualAddress
    pe_print32 IMAGE_SECTION_HEADER.SizeOfRawData
    pe_print32 IMAGE_SECTION_HEADER.PointerToRawData
    pe_print32 IMAGE_SECTION_HEADER.Characteristics

    add si, IMAGE_SECTION_HEADER_size
    add dx, 1
    jmp .loop
.done:

    ret

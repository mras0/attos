SMAP_ID             EQU 0x534D4150 ; 'SMAP'
SMAP_ENTRY_MIN_SIZE EQU 20
SMAP_BUFFER_SIZE    EQU 1024

smap_buffer db SMAP_BUFFER_SIZE

%macro print_reg32 1 ; requires ss=ds
    push esi
    push %1
    mov si, sp
%defstr %%reg %1
    print_lit %%reg, ' = 0x'
    call put_hex32
    call put_crlf
    pop %1
    pop esi
%endmacro

print_regs:
    print_reg32 EAX
    print_reg32 EBX
    print_reg32 ECX
    print_reg32 EDX
    print_reg32 ESI
    print_reg32 EDI
    ret

smap_init:
    pushad

    print_lit 'Getting SMAP', 13, 10

    xor ebx, ebx                                  ; continuation id - 0 to begin
    mov di, smap_buffer                           ; es:di = buffer
    mov esi, SMAP_BUFFER_SIZE-SMAP_ENTRY_MIN_SIZE ; esi   = remaining buffer (leaving room for the terminating entry)
.next:
    cmp esi, SMAP_ENTRY_MIN_SIZE ; abort if we don't have enough room for another entry
    jc .illegal

    mov eax, 0xE820
    mov ecx, esi
    mov edx, SMAP_ID

    int 0x15

    ; Check for error (indicated by the carry flag)
    jc .error

    ; On success eax should be 'SMAP'
    cmp eax, SMAP_ID
    jne .illegal

    ; Are we done?
    and ebx, ebx
    jz .done

    ; Too small?
    cmp ecx, 20
    jb .illegal

    ; We only care about the first part
    sub esi, SMAP_ENTRY_MIN_SIZE
    jb .illegal

    ; Advance buffer pointer
    add di, cx
    jmp .next

.illegal:
    print_lit 'Illegal SMAP entry', 13, 10
    jmp .errout
.error:
    print_lit 'Error!', 13, 10
.errout:
    call print_regs
    jmp exit

.done:
    ; Store terminator
    mov cx, SMAP_ENTRY_MIN_SIZE
    xor ax, ax
    rep stosb

    print_lit 'Base    Length   Type', 13, 10

    ; Print list
    mov si, smap_buffer
.inner:
    cmp dword [si+16], 0 ; Type = 0 -> Done
    je .listend

    ; di <- address of next entry
    lea di, [si+SMAP_ENTRY_MIN_SIZE]

    call put_hex64
    call put_space
    add si, 8

    call put_hex64
    call put_space
    add si, 8

    call put_hex32
    call put_crlf

    mov si, di ; advance to next entry
    jmp .inner

.listend:
    ;get key
    xor ax,ax
    int 0x16

    popad
    ret

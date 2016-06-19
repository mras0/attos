a20_test:
    push ax

    call check_a20
    push ax

    call print_a20_status

    print_lit 'Enabling A20', 13, 10
    mov al, 1
    call set_a20_status
    call print_a20_status

    print_lit 'Disabling A20', 13, 10
    mov al, 0
    call set_a20_status
    call print_a20_status

    print_lit 'Enabling A20', 13, 10
    mov al, 1
    call set_a20_status
    call print_a20_status

    print_lit 'Restoring A20 status', 13, 10
    pop ax
    call set_a20_status
    call print_a20_status

    pop ax
    ret

print_a20_status:
    print_lit 'A20 Enabled: '
    call check_a20
    call put_digit
    call put_crlf
    ret

;
; PS/2 Controller (8042)
;

PS2_DATA_PORT    equ 0x60
PS2_STATUS_PORT  equ 0x64 ; When reading
PS2_COMMAND_PORT equ 0x64 ; When writing

; Status register flags
PS2S_OUTPUT_FULL equ 0x01
PS2S_INPUT_FULL  equ 0x02

; Commands
PS2C_DISABLE_FIRST_PORT           equ 0xAD
PS2C_ENABLE_FIRST_PORT            equ 0xAE
PS2C_READ_CONTROLLER_OUTPUT_PORT  equ 0xD0
PS2C_WRITE_CONTROLLER_OUTPUT_PORT equ 0xD1

; Control output port flags
PS2CO_SYSTEM_RESET equ 0x01
PS2CO_A20_GATE     equ 0x02

; Wait for status bit(s) to clear
; Arguments: test instruction, bit mask
%macro ps2_wait 2
    push ax
    push cx
    mov cx, 10000
%%poll:
    in al, PS2_STATUS_PORT
    test al, %2
    %1 %%done
    dec cx
    %1 %%poll
%defstr %%mask %2
    print_lit 'ps2_wait failed for ', %%mask, 13, 10
    jmp exit
%%done:
    pop cx
    pop ax
%endmacro

ps2_wait_input:
    ps2_wait jz, PS2S_INPUT_FULL
    ret


ps2_wait_output:
    ps2_wait jnz, PS2S_OUTPUT_FULL
    ret

; al = data byte
ps2_write_data:
    call ps2_wait_input
    out PS2_DATA_PORT, al
    ret

; al = command byte
ps2_write_command:
    call ps2_wait_input
    out PS2_COMMAND_PORT, al
    ret

; returns data byte in al
ps2_read_data:
    call ps2_wait_output
    in al, PS2_DATA_PORT
    ret

; returns status byte in al
ps2_read_status:
    call ps2_wait_output
    in al, PS2_STATUS_PORT
    ret

; al = 0 = disable, 1= enable
ps2_a20_control:
    pushf
    cli
    push ax
    push bx
    mov bx, ax

    mov al, PS2C_DISABLE_FIRST_PORT
    call ps2_write_command

    mov al, PS2C_READ_CONTROLLER_OUTPUT_PORT
    call ps2_write_command

    call ps2_read_data
    push ax

    mov al, PS2C_WRITE_CONTROLLER_OUTPUT_PORT
    call ps2_write_command

    pop ax
    and al, ~PS2CO_A20_GATE
    cmp bl, 0
    je  .doset
    or  al, PS2CO_A20_GATE
.doset:
    call ps2_write_data

    mov al, PS2C_ENABLE_FIRST_PORT
    call ps2_write_command

    call ps2_wait_input

    pop bx
    pop ax
    popf
    ret

%macro is_a20_done 0
    call check_a20
    cmp bl, al
    je .done
%endmacro

%macro a20_log 1
    ;print_lit ' [A20] ', %1, 13, 10
%endmacro

; al: 0 = disabled, 1 = enabled
set_a20_status:
    pushf
    pusha
    mov bx, ax ; save requested state in bx (needed for is_a20_done)

    ; Is the a20 line already as expected?
    is_a20_done

    ; Try the bios functions 0x2400 (disable a20 gate) / 0x2401 (enable a20 gate)
    a20_log 'Using BIOS functions'
    and al, 0x01
    mov ah, 0x24
    int 0x15
    is_a20_done

    a20_log 'Using the PS/2 Controller (8042)'
    mov al, bl
    call ps2_a20_control
    is_a20_done

    ; All methods failed. Give up.
    print_lit 'Failed to set a20.', 13, 10
    jmp exit
.done:
    popa
    popf
    ret

; Adapted from http://wiki.osdev.org/A20_Line
;
; Purpose: to check the status of the a20 line in a completely self-contained state-preserving way.
;          The function can be modified as necessary by removing push's at the beginning and their
;          respective pop's at the end if complete self-containment is not required.
;
; Returns: 0 in ax if the a20 line is disabled (memory wraps around)
;          1 in ax if the a20 line is enabled (memory does not wrap around)
check_a20:
    pushf
    push ds
    push es
    push di
    push si

    cli

    xor ax, ax ; ax = 0
    mov es, ax

    not ax ; ax = 0xFFFF
    mov ds, ax

    mov di, 0x0500
    mov si, 0x0510

    mov al, byte [es:di]
    push ax

    mov al, byte [ds:si]
    push ax

    mov byte [es:di], 0x00
    mov byte [ds:si], 0xFF

    cmp byte [es:di], 0xFF

    pop ax
    mov byte [ds:si], al

    pop ax
    mov byte [es:di], al

    mov ax, 0
    je .exit

    mov ax, 1
.exit:
    pop si
    pop di
    pop es
    pop ds
    popf
    ret


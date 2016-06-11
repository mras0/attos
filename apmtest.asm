    org 0x7e00

    jmp main

%include "asmutil.asm"

APM_ALL_DEVICES_ID_V1_1 equ 0x0001
APM_SYSTEM_STATE_OFF equ 0x0003

%macro apm_call 1
    mov ax, 0x5300 | %1
    int 0x15
%endmacro

%macro apm_call_checked 1
    apm_call %1
    jc apm_failure
%endmacro

; al = APM function, ah = error code
apm_failure:
    push ax
    mov si, apm_fail_text1
    call put_string
    call put_hex8
    mov si, apm_fail_text2
    call put_string
    pop ax
    mov al, ah
    call put_hex8
    call put_crlf
    jmp hang

apm_fail_text1 db 'APM call 0x', 0
apm_fail_text2 db ' failed. Error code = 0x', 0

main:
    print_lit 'Hello world',13,10

    ; Get key
    xor ax, ax
    int 0x16

    ;
    ; Power off
    ;

    ; APM installation check
    xor bx, bx ; device id (0 = APM BIOS)
    apm_call_checked 0x00

    call put_hex16
    call put_crlf

    mov al, bh
    call put_char
    mov al, bl
    call put_char
    call put_crlf

    mov ax, cx
    call put_hex16
    call put_crlf

    ; Disconect from any APM interface
    xor bx, bx ; device id (0 = APM BIOS)
    apm_call 0x04
    jnc .apmconnect
    cmp ah, 3 ; interface not connected
    jne apm_failure
.apmconnect:

    ; Connect to real mode interface (initially in v1.0 mode)
    xor bx, bx ; device id (0 = APM BIOS)
    apm_call_checked 0x01

    ; Driver version
    xor bx, bx ; device id (0 = APM BIOS)
    mov cx, 0x0102 ; APM v1.2
    apm_call_checked 0x0E

    ; Enable power management for all devices
    mov bx, APM_ALL_DEVICES_ID_V1_1 ; bx = device id
    mov cx, 1 ; cx = enable state
    apm_call_checked 0x08

    ; Set power state
    mov bx, APM_ALL_DEVICES_ID_V1_1 ; bx = device id
    mov cx, APM_SYSTEM_STATE_OFF ; cx = system state id
    apm_call_checked 0x07

hang:
    hlt
    jmp hang

    align 512,db 0 ; Pad to sector size

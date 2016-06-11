    org 0x7e00

    jmp main

%include "asmutil.asm"

main:
    print_lit 'Hello world',13,10

    call a20_test

exit:
    print_lit 13, 10, 'Done. Press any key to power off.', 13, 10
    ; Get key
    xor ax, ax
    int 0x16

    call power_off

hang:
    print_lit 13, 10, 'Hanging.', 13, 10
.halt:
    hlt
    jmp .halt

%include "a20.asm"
%include "poweroff.asm"
;
; End
;

    times (16*63*512)-($-$$) db 0 ; Make sure we fill at least one cylinder

    align 512, db 0 ; Pad to sector size

%include "..\bootloader\bootcommon.inc"
incbin "..\bootloader\stage2.bin"
incbin "kernel.exe"
incbin "kernel.map.bin"
db 0
incbin "..\exp\userexe\userexe.exe"
%assign size $-$$
%assign max_size (MAX_SECTORS * 512)
%if size > max_size
    %error kernel is too large: size > max_size
%endif
times (16*63*512)-($-$$) db 0

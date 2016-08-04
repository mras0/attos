@pushd %~dp0
@setlocal
@call ..\setflags.cmd
nasm -f win64 -I ..\ ..\attos\isr_common.asm -o isr_common.obj || (popd & exit /b 1)
nasm -f win64 -I ..\ ..\attos\crt.asm -o crt.obj || (popd & exit /b 1)
cl %ATTOS_CXXFLAGS% stage3.cpp isr_common.obj crt.obj ..\attos\attos.lib  /link%ATTOS_LDFLAGS% /nodefaultlib /entry:stage3_entry /subsystem:NATIVE /FILEALIGN:4096 /BASE:0xFFFFFFFFFF000000 /merge:.rdata=.data /merge:.text=.data /merge:.pdata=.data /merge:.xdata:=.data /merge:.CRT=.bss /map || (popd & exit /b 1)
call parse_map.cmd stage3.map > stage3.map.bin || (popd & exit /b 1)
echo>stage3_bin.asm incbin "stage3.exe"|| (popd & exit /b 1)
echo>>stage3_bin.asm incbin "stage3.map.bin"|| (popd & exit /b 1)
rem Ensure zero byte at the end of the map file data
echo>>stage3_bin.asm db 0||(popd & exit/b 1)
rem Pad disk image to fill at least one cylinder
echo>>stage3_bin.asm times (16*63*512)-($-$$) db 0|| (popd & exit /b 1)
nasm -f bin -o stage3.bin stage3_bin.asm || (popd & exit /b 1)
@endlocal
@popd

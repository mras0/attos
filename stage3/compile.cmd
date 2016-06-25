@pushd %~dp0
cl /GS- /GR- /W4 /WX /Os /I. /FAs /D_HAS_EXCEPTIONS=0 stage3.cpp attos\mem.cpp attos\rt.cpp attos\out_stream.cpp attos\vga\text_screen.cpp /link /nodefaultlib /entry:stage3_entry /subsystem:NATIVE /FILEALIGN:4096 /BASE:0xFFFFFFFFFF000000 /DYNAMICBASE:NO /HIGHENTROPYVA:NO /OPT:REF /OPT:ICF /SAFESEH:NO  /merge:.rdata=.data /merge:.text=.data /merge:.pdata=.data /merge:.CRT=.bss || (popd & exit /b 1)
echo>stage3_bin.asm incbin "stage3.exe" || (popd & exit /b 1)
rem Pad disk image to fill at least one cylinder
echo>>stage3_bin.asm times (16*63*512)-($-$$) db 0 || (popd & exit /b 1)
nasm -f bin -o stage3.bin stage3_bin.asm || (popd & exit /b 1)
@popd

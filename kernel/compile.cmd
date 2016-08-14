@pushd %~dp0
@setlocal
@call ..\setflags.cmd
nasm %ATTOS_ASFLAGS% isr_common.asm -o isr_common.obj || (popd & exit /b 1)
nasm %ATTOS_ASFLAGS% cpu_manager_util.asm -o cpu_manager_util.obj || (popd & exit /b 1)
cl %ATTOS_CXXFLAGS% /FAs kernel.cpp cpu_manager.cpp mm.cpp isr.cpp pci.cpp ps2.cpp ata.cpp i825x.cpp text_screen.cpp isr_common.obj cpu_manager_util.obj ..\attos\attos.lib  /link%ATTOS_LDFLAGS% /nodefaultlib /entry:stage3_entry /subsystem:NATIVE /FILEALIGN:4096 /BASE:0xFFFFFFFFFF000000 /merge:.pdata=.rdata /merge:.xdata:=.rdata /merge:.CRT=.rdata /map || (popd & exit /b 1)
call parse_map.cmd kernel.map > kernel.map.bin || (popd & exit /b 1)
echo>kernel_bin.asm incbin "kernel.exe"|| (popd & exit /b 1)
echo>>kernel_bin.asm incbin "kernel.map.bin"|| (popd & exit /b 1)
rem Ensure zero byte at the end of the map file data
echo>>kernel_bin.asm db 0||(popd & exit/b 1)
echo>>kernel_bin.asm incbin "..\exp\userexe\userexe.exe"|| (popd & exit /b 1)
echo>>kernel_bin.asm %%if $-$$ ^> (0x141 * 512)
echo>>kernel_bin.asm %%error too large
echo>>kernel_bin.asm %%endif
rem Pad disk image to fill at least one cylinder
echo>>kernel_bin.asm times (16*63*512)-($-$$) db 0|| (popd & exit /b 1)
nasm -f bin -o kernel.bin kernel_bin.asm || (popd & exit /b 1)
@endlocal
@popd

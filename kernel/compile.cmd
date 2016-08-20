@pushd %~dp0
@setlocal
@call ..\setflags.cmd
nasm %ATTOS_ASFLAGS% isr_common.asm -o isr_common.obj || (popd & exit /b 1)
nasm %ATTOS_ASFLAGS% cpu_manager_util.asm -o cpu_manager_util.obj || (popd & exit /b 1)
cl %ATTOS_CXXFLAGS% /FAs kernel.cpp cpu_manager.cpp mm.cpp isr.cpp pci.cpp ps2.cpp ata.cpp i825x.cpp text_screen.cpp isr_common.obj cpu_manager_util.obj ..\attos\attos_kernel.lib  /link%ATTOS_LDFLAGS% /nodefaultlib /entry:stage3_entry /subsystem:NATIVE /FILEALIGN:4096 /BASE:0xFFFFFFFFFF000000 /merge:.pdata=.rdata /merge:.xdata:=.rdata /merge:.CRT=.rdata /map || (popd & exit /b 1)
call parse_map.cmd kernel.map > kernel.map.bin || (popd & exit /b 1)
nasm -f bin -o kernel.bin kernel_bin.asm || (popd & exit /b 1)
@endlocal
@popd

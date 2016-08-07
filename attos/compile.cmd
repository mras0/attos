@pushd %~dp0
@setlocal
@call ..\setflags.cmd
@set cpp=rt.cpp cpu.cpp mem.cpp mm.cpp isr.cpp pci.cpp ata.cpp pe.cpp out_stream.cpp
@set extracpp=vga\text_screen.cpp net\net.cpp net\i825x.cpp net\tftp.cpp
@set extraobj=text_screen.obj net.obj i825x.obj tftp.obj isr_common.obj crt.obj
nasm %ATTOS_ASFLAGS% isr_common.asm -o isr_common.obj || (popd & exit /b 1)
nasm %ATTOS_ASFLAGS% crt.asm -o crt.obj || (popd & exit /b 1)
cl %ATTOS_CXXFLAGS% /c %cpp% %extracpp% || (popd & exit /b 1)
lib /out:attos.lib /nologo /ignore:4221 %cpp:.cpp=.obj% %extraobj% || (popd & exit /b 1)
@endlocal
@popd

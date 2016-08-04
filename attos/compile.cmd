@pushd %~dp0
@setlocal
@call ..\setflags.cmd
@set cpp=rt.cpp cpu.cpp mem.cpp mm.cpp isr.cpp pci.cpp ata.cpp pe.cpp out_stream.cpp
@set vgacpp=vga\text_screen.cpp
@set vgaobj=text_screen.obj
@set netcpp=net\net.cpp net\i825x.cpp net\tftp.cpp
@set netobj=net.obj i825x.obj tftp.obj
cl %ATTOS_CXXFLAGS% /c %cpp%
lib /out:attos.lib /nologo /ignore:4221 %cpp:.cpp=.obj% %vgaobj% %netobj%
@endlocal
@popd

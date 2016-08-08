@pushd %~dp0
@setlocal
@call ..\setflags.cmd
@set cpp=rt.cpp cpu.cpp mem.cpp mm.cpp isr.cpp pci.cpp ata.cpp pe.cpp out_stream.cpp
@set extracpp=net\net.cpp net\tftp.cpp
@set hostcpp=host_stubs.cpp
@set obj=%cpp:.cpp=.obj% net.obj tftp.obj
@set targetobj=crt.obj
nasm %ATTOS_ASFLAGS% crt.asm -o crt.obj || (popd & exit /b 1)
cl %ATTOS_CXXFLAGS% /c %cpp% %extracpp% %hostcpp% || (popd & exit /b 1)
lib /out:attos_host.lib /nologo /ignore:4221 %obj% %hostcpp:.cpp=.obj% || (popd & exit /b 1)
lib /out:attos.lib /nologo /ignore:4221 %obj% %targetobj% || (popd & exit /b 1)
@endlocal
@popd

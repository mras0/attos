@setlocal
@pushd %~dp0
call ..\..\setflags.cmd
nasm %ATTOS_ASFLAGS% test.asm || exit /b 1
link %ATTOS_LDFLAGS% /subsystem:native /nodefaultlib /entry:main /merge:.rdata=.text test.obj || exit /b 1
cl %ATTOS_CXXFLAGS% tftps.cpp ..\..\attos\attos_host.lib ws2_32.lib /link /nodefaultlib:memcpy.obj || exit /b 1
endlocal
popd

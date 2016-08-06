@pushd %~dp0
@setlocal
@call ..\..\setflags.cmd || (popd & exit /b 1)
nasm %ATTOS_ASFLAGS% syscall.asm -o syscall.obj || (popd & exit /b 1)
cl %ATTOS_CXXFLAGS% userexe.cpp crtstartup.cpp syscall.obj ..\..\attos\attos.lib /link %ATTOS_LDFLAGS% /nodefaultlib || (popd & exit /b 1)
@endlocal
@popd

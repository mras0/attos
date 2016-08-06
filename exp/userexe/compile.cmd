@pushd %~dp0
@setlocal
@call ..\..\setflags.cmd || (popd & exit /b 1)
nasm %ATTOS_ASFLAGS% usercrt.asm -o usercrt.obj || (popd & exit /b 1)
cl %ATTOS_CXXFLAGS% userexe.cpp ..\..\attos\attos.lib usercrt.obj /link %ATTOS_LDFLAGS% /nodefaultlib || (popd & exit /b 1)
@endlocal
@popd

@pushd %~dp0
@setlocal
@call ..\..\setflags.cmd || (popd & exit /b 1)
cl %ATTOS_CXXFLAGS% userexe.cpp ..\..\attos\attos_user.lib /link %ATTOS_LDFLAGS% /nodefaultlib || (popd & exit /b 1)
@endlocal
@popd

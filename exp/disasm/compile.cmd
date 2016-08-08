@setlocal
@pushd %~dp0
call ..\..\setflags.cmd
cl %ATTOS_CXXFLAGS% disasm.cpp ..\..\attos\attos_host.lib || exit /b 1
endlocal
popd

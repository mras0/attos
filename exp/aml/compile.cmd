@setlocal
@pushd %~dp0
call ..\..\setflags.cmd
cl %ATTOS_CXXFLAGS% aml.cpp ..\..\attos\attos_host.lib /link /nodefaultlib:memcpy.obj || exit /b 1
@endlocal
@popd

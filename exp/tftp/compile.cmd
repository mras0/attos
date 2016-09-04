@setlocal
@pushd %~dp0
call ..\..\setflags.cmd
cl %ATTOS_CXXFLAGS% /I..\aml test.cpp ..\aml\aml.cpp ..\..\attos\attos_user.lib /link %ATTOS_LDFLAGS% /nodefaultlib || exit /b 1
cl %ATTOS_CXXFLAGS% tftps.cpp ..\..\attos\attos_host.lib ws2_32.lib /link /nodefaultlib:memcpy.obj || exit /b 1
endlocal
popd

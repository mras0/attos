@call ..\..\setflags.cmd
cl %ATTOS_CXXFLAGS% tftps.cpp ..\..\attos\attos_host.lib ws2_32.lib /link /nodefaultlib:memcpy.obj || exit /b 1

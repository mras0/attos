@call ..\..\setflags.cmd
cl %ATTOS_CXXFLAGS% tftps.cpp ..\..\attos\attos.lib ws2_32.lib || exit /b 1

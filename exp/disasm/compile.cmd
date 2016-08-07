@call ..\..\setflags.cmd
cl %ATTOS_CXXFLAGS% disasm.cpp ..\..\attos\attos_host.lib || exit /b 1

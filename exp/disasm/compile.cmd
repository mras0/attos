@call ..\..\setflags.cmd
cl %ATTOS_CXXFLAGS% disasm.cpp ..\..\attos\attos.lib || exit /b 1

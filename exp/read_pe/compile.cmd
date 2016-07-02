@setlocal
@set ad=..\..\stage3

cl /W4 /WX /DEBUG /Zi /D_HAS_EXCEPTIONS=0 /FAs /I%ad% read_pe.cpp %ad%\attos\pe.cpp %ad%\attos\out_stream.cpp || exit /b 1
@rem dumpbin /disasm read_pe.exe > read_pe.asm || exit /b 1

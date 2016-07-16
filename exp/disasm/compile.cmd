@setlocal
@set ad=..\..\stage3
cl /W4 /WX /DEBUG /Zi /D_HAS_EXCEPTIONS=0 /I%ad% disasm.cpp %ad%\attos\pe.cpp %ad%\attos\out_stream.cpp || exit /b 1

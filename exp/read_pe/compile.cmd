@setlocal
@set ad=..\..\stage3
nasm -f win64 asm_test.asm -o asm_test.obj || (popd & exit /b 1)
cl /W4 /WX /DEBUG /Zi /D_HAS_EXCEPTIONS=0 /FAs /I%ad% read_pe.cpp %ad%\attos\pe.cpp %ad%\attos\out_stream.cpp asm_test.obj || exit /b 1
@rem dumpbin /disasm read_pe.exe > read_pe.asm || exit /b 1

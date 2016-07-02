cl /W4 /WX /DEBUG /Zi /D_HAS_EXCEPTIONS=0 /FAs read_pe.cpp ..\..\stage3\attos\pe.cpp || exit /b 1
@rem dumpbin /disasm read_pe.exe > read_pe.asm || exit /b 1

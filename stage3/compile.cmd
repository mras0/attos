@pushd %~dp0
cl /Fa /FAs /GS- /GR- /W4 /WX /Os /I. stage3.cpp attos\out_stream.cpp attos\vga\text_screen.cpp /link /nodefaultlib /entry:small_exe /subsystem:NATIVE /FILEALIGN:4096 /DYNAMICBASE:NO /HIGHENTROPYVA:NO /OPT:REF /OPT:ICF /SAFESEH:NO || (popd & exit /b 1)
@popd

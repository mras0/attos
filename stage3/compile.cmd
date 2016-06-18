@pushd %~dp0
cl /Fa /FAs /GS- /W4 /WX /Os stage3.cpp /link /nodefaultlib /entry:small_exe /subsystem:NATIVE /FILEALIGN:4096 /DYNAMICBASE:NO /HIGHENTROPYVA:NO /OPT:REF /OPT:ICF /SAFESEH:NO || (popd & exit /b 1)
@popd

@pushd %~dp0
cl /W4 /WX /Ox stage3.cpp /link /nodefaultlib /entry:small_exe /subsystem:NATIVE /FILEALIGN:4096 /DYNAMICBASE:NO /HIGHENTROPYVA:NO || (popd & exit /b 1)
@popd

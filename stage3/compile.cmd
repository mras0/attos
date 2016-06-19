@pushd %~dp0
cl /GS- /GR- /W4 /WX /Os /I. /favor:ATOM stage3.cpp attos\out_stream.cpp attos\vga\text_screen.cpp /link /nodefaultlib /entry:small_exe /subsystem:NATIVE /FILEALIGN:4096 /BASE:0xFFFFFFFFFF000000 /DYNAMICBASE:NO /HIGHENTROPYVA:NO /OPT:REF /OPT:ICF /SAFESEH:NO  /merge:.rdata=.data /merge:.text=.data /merge:.pdata=.data || (popd & exit /b 1)
@popd

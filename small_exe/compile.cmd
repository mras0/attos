@pushd %~dp0
cl /W4 /WX /Ox small_exe.cpp /link /nodefaultlib /entry:small_exe /subsystem:NATIVE || (popd & exit /b 1)
@popd

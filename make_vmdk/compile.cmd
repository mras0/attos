pushd %~dp0
cl /W4 /WX make_vmdk.c || (popd & exit /b 1)
popd

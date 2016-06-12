pushd %~dp0
cl /W4 /WX make_vmdk.c || (popd & exit /b 1)
nasm -f bin stage1.asm -o stage1.bin || (popd & exit /b 1)
call small_exe\compile.cmd || (popd & exit /b 1)
nasm -f bin stage2.asm -o stage2.bin || (popd & exit /b 1)
cd test-vm
copy /b /y ..\stage1.bin+..\stage2.bin test-vm.raw || (popd & exit /b 1)
..\make_vmdk.exe test-vm.raw > test-vm.vmdk || (popd & exit /b 1)
popd

pushd %~dp0
call bootloader\compile.cmd || (popd & exit /b 1)
call attos\compile.cmd || (popd & exit /b 1)
call exp\userexe\compile.cmd || (popd & exit /b 1)
call kernel\compile.cmd || (popd & exit /b 1)
call make_vmdk\compile.cmd || (popd & exit /b 1)
cd test-vm
copy /b /y ..\bootloader\stage1.bin+..\kernel\kernel.bin test-vm.raw || (popd & exit /b 1)
..\make_vmdk\make_vmdk.exe test-vm.raw > test-vm.vmdk || (popd & exit /b 1)
popd

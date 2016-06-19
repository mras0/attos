pushd %~dp0
call stage3\compile.cmd || (popd & exit /b 1)
call bootloader\compile.cmd || (popd & exit /b 1)
call make_vmdk\compile.cmd || (popd & exit /b 1)
cd test-vm
copy /b /y ..\bootloader\stage1.bin+..\bootloader\stage2.bin test-vm.raw || (popd & exit /b 1)
..\make_vmdk\make_vmdk.exe test-vm.raw > test-vm.vmdk || (popd & exit /b 1)
popd
"c:\Program Files (x86)\VMware\VMware Player\vmplayer.exe" "%~dp0test-vm\test-vm.vmx" || exit /b 1

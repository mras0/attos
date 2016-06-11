pushd %~dp0
call compile.cmd || (popd & exit /b 1)
cd test-vm
copy /y ..\boot test-vm.raw || (popd & exit /b 1)
..\make_vmdk.exe || (popd & exit /b 1)
popd
"c:\Program Files (x86)\VMware\VMware Player\vmplayer.exe" "%~dp0test-vm\test-vm.vmx" || exit /b 1

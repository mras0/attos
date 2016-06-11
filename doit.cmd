pushd %~dp0
call compile.cmd
cd test-vm
copy /y ..\boot test-vm.raw
..\make_vmdk.exe
popd
"c:\Program Files (x86)\VMware\VMware Player\vmplayer.exe" "%~dp0test-vm\test-vm.vmx"

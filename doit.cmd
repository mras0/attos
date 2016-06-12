call "%~dp0compile.cmd" || (popd & exit /b 1)
"c:\Program Files (x86)\VMware\VMware Player\vmplayer.exe" "%~dp0test-vm\test-vm.vmx" || exit /b 1

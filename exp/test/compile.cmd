@setlocal
@pushd %~dp0
cl /EHsc /W4 /WX /DEBUG /Zi /I..\..\ test.cpp ..\..\attos\attos_host.lib || exit /b 1
@endlocal
@popd

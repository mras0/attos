@setlocal
@pushd %~dp0
cl /EHsc /W4 /WX /DEBUG /Zi tree.cpp || exit /b 1
@endlocal
@popd

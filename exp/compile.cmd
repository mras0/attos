call "%~dp0\disasm\compile.cmd" || exit /b 1
call "%~dp0\read_pe\compile.cmd" || exit /b 1
call "%~dp0\tftp\compile.cmd" || exit /b 1
call "%~dp0\tree\compile.cmd" || exit /b 1
call "%~dp0\userexe\compile.cmd" || exit /b 1
call "%~dp0\aml\compile.cmd" || exit /b 1

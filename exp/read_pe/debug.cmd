@setlocal

call compile.cmd || exit /b 1

@set sympath=srv*;%~dp0
@set srcpath=%~dp0
@set exe=%~dp0read_pe.exe

cdb -y %sympath% -srcpath %srcpath% -i %exe% -c "$$><1.dbg" %exe%

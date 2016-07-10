@setlocal

call compile.cmd || exit /b 1

@set sympath=srv*;%~dp0
@set srcpath=%~dp0
@set exe=%~dp0disasm.exe

windbg -y %sympath% -srcpath %srcpath% -i %exe% %exe%

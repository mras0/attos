@echo off
setlocal
set sympath="srv*;%~dp0"
set srcpath="%~dp0"
set exe="%~dp0tree.exe"

windbg -y %sympath% -srcpath %srcpath% -i %exe% %exe%


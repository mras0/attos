@echo off
if "%1"=="" echo Usage: %0 map-file&exit/b 1

setlocal enabledelayedexpansion
set state=init

echo 0000000000000000 lowmem

for /f "tokens=* delims=" %%a in (%1) do (
    call :parse_line_!state! %%a
    if "!state!"=="done" goto :done
)
echo Parse failed!
exit /b 1
:done
echo ffffffffffffffff end-of-memory
exit /b 0

:parse_line_init
if "%*"=="Address         Publics by Value              Rva+Base               Lib:Object" set state=read_symbols&goto :eof
rem echo Skipping (waiting for init): %*
goto :eof

:parse_line_read_symbols
if "%1"=="entry" set state=done&goto :eof
echo %3 %2
goto :eof

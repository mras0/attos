@echo off
setlocal
call compile || exit /b 1
if "%1"=="" echo Usage: %0 exe-file & exit /b 1
set pp=%~dp1
set pn=%~n1
set prg=%1
if not exist %pn%.asm (
    objdump -w -z -M intel -d %prg% > %pn%.asm
    sed -i -e "s/\*1//" -e "s/movabs/mov/" -e "s/stos BYTE PTR es:\[rdi\],al/stosb/" %pn%.asm
)
disasm %prg% %2 > %pn%_2.asm && gvimdiff -c "set diffopt+=iwhite,context:2" %pn%_2.asm %pn%.asm

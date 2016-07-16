@setlocal
@set pp=c:\windows\system32
@set pn=ntoskrnl
@set prg=%pp%\%pn%.exe
if not exist %pn%.asm (
    objdump -w -z -M intel -d %prg% > %pn%.asm
    sed -i -e "s/\*1//" -e "s/movabs/mov/" -e "s/stos BYTE PTR es:\[rdi\],al/stosb/" %pn%.asm
)
compile && disasm %prg% > %pn%_2.asm && gvimdiff -c "set diffopt+=iwhite,context:2" %pn%_2.asm %pn%.asm

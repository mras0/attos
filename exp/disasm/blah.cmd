@setlocal
@set prg=c:\windows\notepad.exe
objdump -w -z -M intel -d %prg% > notepad.asm
@rem sed -i -e "s/rex push/push/" -e "s/\*1//" notepad.asm
sed -i -e "s/\*1//" -e "s/movabs/mov/" -e "s/stos BYTE PTR es:\[rdi\],al/stosb/" notepad.asm
compile && disasm %prg% > notepad_2.asm && gvimdiff -c "set diffopt+=iwhite,context:2" notepad_2.asm notepad.asm

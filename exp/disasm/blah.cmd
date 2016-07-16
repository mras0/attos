@setlocal
@set prg=c:\windows\notepad.exe
objdump -w -z -M intel -d %prg% > notepad.asm
compile && disasm %prg% > notepad_2.asm && gvimdiff -c "set diffopt+=iwhite,context:2" notepad_2.asm notepad.asm

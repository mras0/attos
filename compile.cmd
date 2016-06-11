cl /W4 make_vmdk.c || exit /b 1
nasm -f bin boot.asm || exit /b 1

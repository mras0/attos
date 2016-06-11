cl /W4 /WX make_vmdk.c || exit /b 1
nasm -f bin boot.asm || exit /b 1
nasm -f bin apmtest.asm || exit /b 1

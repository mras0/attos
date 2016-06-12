cl /W4 /WX make_vmdk.c || exit /b 1
nasm -f bin stage1.asm -o stage1.bin || exit /b 1
nasm -f bin stage2.asm -o stage2.bin || exit /b 1

pushd %~dp0
nasm -f bin stage1.asm -o stage1.bin || (popd & exit /b 1)
nasm -f bin stage2.asm -o stage2.bin || (popd & exit /b 1)
popd

    BOOT_ADDR equ 0x7c00
    LOAD_ADDR equ 0x7e00

    SCRATCH_ADDRESS EQU 0x00030000
    SCRATCH_SIZE    EQU 0x00090000-SCRATCH_ADDRESS

    MAX_SECTORS equ (SCRATCH_ADDRESS-LOAD_ADDR) / 512

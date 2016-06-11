void small_exe()
{
    *((unsigned short*)0xb8000) = 0x0700 | 'W';
    for (;;) {}
}

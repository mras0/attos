@call compile.cmd
aml.exe bochs_dsdt.aml>nul || exit/b 1
aml.exe vmware_dsdt.aml>nul || exit/b 1
aml.exe x200s_dsdt.aml>nul || exit/b 1

#include <attos/out_stream.h>
#include <attos/sysuser.h>
#include <attos/mem.h>

using namespace attos;

int main()
{
    dbgout() << "test.exe running!\n";
    mem_map_info dsdt_mem;
    sys_handle dsdt{"hack-acpi-dsdt"};
    syscall2(syscall_number::mem_map_info, dsdt.id(), reinterpret_cast<uint64_t>(&dsdt_mem));
    //hexdump(dbgout(), dsdt_mem.addr.in_current_address_space<>(), dsdt_mem.length);
    dbgout() << "Length = " << as_hex(dsdt_mem.length) << "\n";
    return 42;
}

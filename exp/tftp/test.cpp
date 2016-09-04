#include <attos/out_stream.h>
#include <attos/sysuser.h>
#include <attos/mem.h>
#include "aml.h"

using namespace attos;

int main()
{
    dbgout() << "test.exe running!\n";
    mem_map_info dsdt_mem;
    sys_handle dsdt{"hack-acpi-dsdt"};
    syscall2(syscall_number::mem_map_info, dsdt.id(), reinterpret_cast<uint64_t>(&dsdt_mem));
    constexpr auto sizeof_acpi_description = 36;
    acpi::process(make_array_view(dsdt_mem.addr.in_current_address_space<>()+sizeof_acpi_description, dsdt_mem.length-sizeof_acpi_description));
    return 0;
}

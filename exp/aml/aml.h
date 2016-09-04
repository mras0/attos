#ifndef ATTOS_ACPI_AML_H
#define ATTOS_ACPI_AML_H

namespace attos {

class out_stream;

} // namespace attos

#include <attos/array_view.h>

namespace attos { namespace acpi {

void process(array_view<uint8_t> data);

} } // namespace attos::acpi

#endif

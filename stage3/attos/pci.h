#ifndef ATTOS_PCI_H
#define ATTOS_PCI_H

#include <attos/mem.h>

namespace attos { namespace pci {

class __declspec(novtable) manager {
public:
    virtual ~manager() {}
};

owned_ptr<manager, destruct_deleter> init();

} }  // namespace attos::pci

#endif

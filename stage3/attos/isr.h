#ifndef ATTOS_ISR_H
#define ATTOS_ISR_H

#include <attos/mem.h>

namespace attos {

class __declspec(novtable) isr_handler {
public:
    virtual ~isr_handler() {}
};

owned_ptr<isr_handler, destruct_deleter> isr_init();

} // namespace attos

#endif

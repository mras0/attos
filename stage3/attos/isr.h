#ifndef ATTOS_ISR_H
#define ATTOS_ISR_H

#include <attos/mm.h>
#include <attos/function.h>

namespace attos {

class __declspec(novtable) isr_registration {
public:
    virtual ~isr_registration() {}
};
using isr_registration_ptr = kowned_ptr<isr_registration>;

using irq_handler_t = function<void ()>;

class __declspec(novtable) isr_handler {
public:
    virtual ~isr_handler() {}
};

owned_ptr<isr_handler, destruct_deleter> isr_init(char* debug_info_text);

isr_registration_ptr register_irq_handler(uint8_t irq, irq_handler_t irq_handler);

} // namespace attos

#endif

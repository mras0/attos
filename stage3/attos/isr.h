#ifndef ATTOS_ISR_H
#define ATTOS_ISR_H

#include <attos/mm.h>

namespace attos {

class __declspec(novtable) isr_registration {
public:
    virtual ~isr_registration() {}
};

template<typename R, typename... Args>
class function_base {
public:
    function_base() : impl_(nullptr) {
    }

    template<typename F>
    function_base(F f) : impl_(knew<impl>(f)) {
    }

    R operator()(Args&&... args) const {
        return impl_->invoke(static_cast<Args&&>(args)...);
    }

private:
    struct impl {
        virtual R invoke(Args&&... args) const = 0;
    };

    template<typename T>
    struct fimpl;

    kowned_ptr<impl> impl_;
};

using irq_handler_t = void (*)();

class __declspec(novtable) isr_handler {
public:
    virtual ~isr_handler() {}
    kowned_ptr<isr_registration> register_irq_handler(uint8_t irq, irq_handler_t irq_handler) {
        return do_register_irq_handler(irq, irq_handler);
    }

private:
    virtual kowned_ptr<isr_registration> do_register_irq_handler(uint8_t irq, irq_handler_t irq_handler) = 0;
};

owned_ptr<isr_handler, destruct_deleter> isr_init();

} // namespace attos

#endif

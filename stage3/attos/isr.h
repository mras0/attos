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
    function_base(nullptr_t = nullptr) : impl_buffer_() {
    }

    template<typename F>
    function_base(const F& f) {
        new (&impl_buffer_[0]) fimpl<F>(f);
    }

    // TOOD:...
    function_base(const function_base& f) = default;
    function_base& operator=(const function_base& f) = default;
    ~function_base() = default;

    explicit operator bool() const {
        return *reinterpret_cast<const uint64_t*>(impl_buffer_) != 0;
    }

    R operator()(Args&&... args) const {
        return f().invoke(static_cast<Args&&>(args)...);
    }

private:
    struct __declspec(novtable) impl {
        virtual R invoke(Args&&... args) const = 0;
    };

    template<typename F>
    struct fimpl;

    template<>
    struct fimpl<R (*)(Args...)> : impl {
        using Fptr = R (*)(Args...);
        explicit fimpl(Fptr fptr) : fptr_(fptr) {
        }

        virtual R invoke(Args&&... args) const override {
            return fptr_(static_cast<Args&&>(args)...);
        }
    private:
        Fptr fptr_;
    };

    static constexpr size_t impl_max_size = 16;
    alignas(16) uint8_t impl_buffer_[impl_max_size];

    impl& f() const { return *reinterpret_cast<impl*>(const_cast<uint8_t*>(impl_buffer_)); }
};

using irq_handler_t = function_base<void>;

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

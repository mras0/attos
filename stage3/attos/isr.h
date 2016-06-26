#ifndef ATTOS_ISR_H
#define ATTOS_ISR_H

#include <attos/mm.h>

namespace attos {

class __declspec(novtable) isr_registration {
public:
    virtual ~isr_registration() {}
};
using isr_registration_ptr = kowned_ptr<isr_registration>;

template<typename R, typename... Args>
class function_base {
public:
    function_base(nullptr_t = nullptr) {
        reset();
    }

    template<typename F>
    function_base(const F& f) {
        new (&impl_buffer_[0]) fimpl<F>(f);
    }

    function_base(const function_base& other) {
        *this = other;
    }

    function_base& operator=(const function_base& rhs) {
        reset();
        if (rhs) {
            rhs.f().construct(&impl_buffer_[0]);
        }
        return *this;
    }

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
        virtual void construct(void* dest) const = 0;
    };

    template<typename F>
    struct fimpl : impl {
    public:
        explicit fimpl(const F& f) : f_(f) {
            static_assert(sizeof(fimpl) <= impl_max_size, "Too large");
            static_assert(std::is_trivially_destructible_v<fimpl>, "Implementation is too lazy");
        }

        virtual void construct(void* dest) const override {
            new (dest) fimpl(f_);
        }

        virtual R invoke(Args&&... args) const override {
            return f_(static_cast<Args&&>(args)...);
        }

    private:
        F f_;
    };

    static constexpr size_t impl_max_size = 16;
    alignas(16) uint8_t impl_buffer_[impl_max_size];

    impl& f() const { return *reinterpret_cast<impl*>(const_cast<uint8_t*>(impl_buffer_)); }
    void reset() {
        memset(impl_buffer_, 0, sizeof(impl_max_size));
    }
};

using irq_handler_t = function_base<void>;

class __declspec(novtable) isr_handler {
public:
    virtual ~isr_handler() {}
    isr_registration_ptr register_irq_handler(uint8_t irq, irq_handler_t irq_handler) {
        return do_register_irq_handler(irq, irq_handler);
    }

private:
    virtual isr_registration_ptr do_register_irq_handler(uint8_t irq, irq_handler_t irq_handler) = 0;
};

owned_ptr<isr_handler, destruct_deleter> isr_init();

} // namespace attos

#endif

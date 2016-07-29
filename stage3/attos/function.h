#ifndef ATTOS_FUNCTION_H
#define ATTOS_FUNCTION_H

namespace attos {

template<typename>
class function;

template<typename R, typename... Args>
class function<R(Args...)> {
public:
    function(nullptr_t = nullptr) {
        reset();
    }

    template<typename F>
    function(const F& f) {
        new (&impl_buffer_[0]) fimpl<F>(f);
    }

    function(const function& other) {
        *this = other;
    }

    function& operator=(const function& rhs) {
        reset();
        if (rhs) {
            rhs.f().construct(&impl_buffer_[0]);
        }
        return *this;
    }

    ~function() = default;

    explicit operator bool() const {
        return *reinterpret_cast<const uint64_t*>(impl_buffer_) != 0;
    }

    R operator()(Args... args) const {
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

    static constexpr size_t impl_max_size = 32;
    alignas(16) uint8_t impl_buffer_[impl_max_size];

    impl& f() const { return *reinterpret_cast<impl*>(const_cast<uint8_t*>(impl_buffer_)); }
    void reset() {
        memset(impl_buffer_, 0, sizeof(impl_max_size));
    }
};

} // namespace attos

#endif

#include "ps2.h"
#include "isr.h"
#include <attos/cpu.h>
#include <attos/out_stream.h>

namespace attos { namespace ps2 {

constexpr uint8_t data_port    = 0x60;
constexpr uint8_t status_port  = 0x64; // when reading
constexpr uint8_t command_port = 0x64; // when writing

constexpr uint8_t status_mask_output_full      = 0x01;
constexpr uint8_t status_mask_input_fill       = 0x02;
constexpr uint8_t status_mask_system_status    = 0x04;
constexpr uint8_t status_mask_is_command       = 0x08;
constexpr uint8_t status_mask_keyboard_lock    = 0x10;
constexpr uint8_t status_mask_transmit_timeout = 0x20;
constexpr uint8_t status_mask_receive_timeout  = 0x40;
constexpr uint8_t status_mask_parity_error     = 0x80;

constexpr uint8_t sc_invalid = 0;
constexpr uint8_t scan_code_set_1_size = 0x58;
constexpr uint8_t scan_code_set_1[scan_code_set_1_size] = {
    /* 0x00 */ sc_invalid,
    /* 0x01 */ '\x1b', // escape
    /* 0x02 */ '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    /* 0x0c */ '-',
    /* 0x0d */ '=',
    /* 0x0e */ '\x08', // backspace
    /* 0x0f */ '\x09', // tab
    /* 0x10 */ 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\x0A', // (enter) newline
    /* 0x1D */ sc_invalid, // left control
    /* 0x1E */ 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`',
    /* 0x2A */ sc_invalid, // left shift
    /* 0x2B */ '\\',
    /* 0x2C */ 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/',
    /* 0x36 */ sc_invalid, // right shift
    /* 0x37 */ sc_invalid, // keypad *
    /* 0x38 */ sc_invalid, // left alt
    /* 0x39 */ ' ', // space
    /* 0x3A */ sc_invalid, // caps lock
    /* 0x3B */ sc_invalid, // f1
    /* 0x3C */ sc_invalid, // f2
    /* 0x3D */ sc_invalid, // ...
    /* 0x3E */ sc_invalid,
    /* 0x3F */ sc_invalid,
    /* 0x40 */ sc_invalid,
    /* 0x41 */ sc_invalid,
    /* 0x42 */ sc_invalid,
    /* 0x43 */ sc_invalid,
    /* 0x44 */ sc_invalid, // f10
    /* 0x45 */ sc_invalid, // numlock
    /* 0x46 */ sc_invalid, // scolllock
    /* 0x47 */ '7', // keypad 7
    /* 0x48 */ '8', // keypad 8
    /* 0x49 */ '9', // keypad 9
    /* 0x4A */ '-', // keypad -
    /* 0x4B */ '4', // keypad 4
    /* 0x4C */ '5', // keypad 5
    /* 0x4D */ '6', // keypad 6
    /* 0x4E */ '+', // keypad +
    /* 0x4F */ '1', // keypad 1
    /* 0x50 */ '2', // keypad 2
    /* 0x51 */ '3', // keypad 3
    /* 0x52 */ '0', // keypad 0
    /* 0x53 */ '.', // keypad .
    /* 0x54 */ sc_invalid,
    /* 0x55 */ sc_invalid,
    /* 0x56 */ sc_invalid,
    /* 0x57 */ sc_invalid, // f11
    /* 0x58 */ 'Z', // f12
};

uint8_t data() {
    return __inbyte(ps2::data_port);
}

uint8_t status() {
    return __inbyte(ps2::status_port);
}

template<typename T, size_t Size>
class fixed_size_queue {
public:
    fixed_size_queue() {
        static_assert((Size & (Size-1)) == 0, "");
        static_assert(std::is_trivially_copyable_v<T>, "");
    }

    bool empty() const {
        return size() == 0;
    }

    bool full() const {
        return size() == Size;
    }

    size_t size() const {
        return write_pos_ - read_pos_;
    }

    void push(T elem) {
        elems_[(write_pos_++) & (Size - 1)] = elem;
    }

    T pop() {
        return elems_[(read_pos_++) & (Size - 1)];
    }

    class iterator {
    public:
        bool operator==(const iterator& rhs) const { return pos_ == rhs.pos_; }
        bool operator!=(const iterator& rhs) const { return !(*this == rhs); }

        iterator& operator++() {
            ++pos_;
            return *this;
        }

        auto* operator->() {
            return &*this;
        }

        auto& operator*() {
            return parent_->elems_[pos_ & (Size - 1)];
        }

    private:
        iterator(const fixed_size_queue& parent, uint32_t pos) : parent_(&parent), pos_(pos) {}
        friend fixed_size_queue;
        const fixed_size_queue* parent_;
        uint32_t pos_;
    };

    iterator begin() const { return iterator{*this, read_pos_}; }
    iterator end() const { return iterator{*this, write_pos_}; }

private:
    T elems_[Size];
    uint32_t read_pos_  = 0;
    uint32_t write_pos_ = 0;
};

class controller_impl : public controller, public singleton<controller_impl> {
public:
    explicit controller_impl() {
        reg_ = register_irq_handler(irq, [this]() { isr(); });
    }

    virtual ~controller_impl() override {
        reg_.reset();
    }

    bool key_available() const {
        interrupt_disabler id{};
        // Peek to see if there any key releases
        return std::any_of(buffer_.begin(), buffer_.end(), [](uint8_t c) { return c > 0x80 && c < 0xE0; });
    }

    uint8_t read_key() {
        // Very crude translation of scan keys in Scan Code Set 1
        for (;;) {
            auto sk = get_scan_key();
            if (sk == 0xE0) {
                auto sk2 = get_scan_key();
                dbgout() << "[ps2] ignoring scan key = 0xE0 0x" << as_hex(sk2) << "\n";
                continue;
            } else if (sk == 0xE1) {
                auto sk2 = get_scan_key();
                auto sk3 = get_scan_key();
                dbgout() << "[ps2] ignoring scan key = 0xE1 0x" << as_hex(sk2) << " 0x" << as_hex(sk3) << "\n";
                continue;
            }

            // Only act on key release as they are not repeated
            const bool released = (sk & 0x80) != 0;
            if (!released) continue;
            sk &= ~0x80;

            if (sk < scan_code_set_1_size) {
                const auto k = scan_code_set_1[sk];
                if (k != sc_invalid) {
                    return k;
                }
            }
            dbgout() << "[ps2] ignoring scan key = 0x" << as_hex(sk) << "\n";
        }
    }

private:
    static constexpr uint8_t irq = 1;

    isr_registration_ptr reg_;
    fixed_size_queue<uint8_t, 32> buffer_;

    bool buffer_empty() {
        interrupt_disabler id{};
        return buffer_.empty();
    }

    uint8_t get_scan_key() {
        while (buffer_empty()) {
            __halt();
        }
        interrupt_disabler id{};
        return buffer_.pop();
    }


    void isr() {
        const auto st = status();
        REQUIRE(st & status_mask_output_full);

        if (!buffer_.full()) {
            buffer_.push(data());
        } else {
            dbgout() << "[ps2] Keyboard buffer is full!!\n";
        }
    }
};

object_buffer<controller_impl> controller_buffer;
owned_ptr<controller, destruct_deleter> init() {
    return owned_ptr<controller, destruct_deleter>{controller_buffer.construct().release()};
}

bool key_available() {
    if (controller_impl::has_instance()) {
        return controller_impl::instance().key_available();
    }
    return status() & status_mask_output_full;
}

uint8_t read_key() {
    if (controller_impl::has_instance()) {
        return controller_impl::instance().read_key();
    }

    for (;;) {
        while (!(status() & status_mask_output_full)) { // wait key
            _mm_pause();
        }
        const uint8_t scan_code = data();  // read key
        if (scan_code > 0x80 && scan_code < 0xE0) { // crude wait for release of 'normal' key
            return scan_code;
        }
    }
}

} } // namespace attos::ps2

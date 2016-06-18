#include <stdint.h>
#include <intrin.h>
#include <type_traits>

// http://www.catch22.net/tuts/reducing-executable-size
// Merge all default sections into the .text (code) section.
#pragma comment(linker,"/merge:.rdata=.data")
#pragma comment(linker,"/merge:.text=.data")
#pragma comment(linker,"/merge:.pdata=.data")
//#pragma comment(linker,"/merge:.reloc=.data")

const unsigned char bochs_magic_code[] = { 0x66, 0x87, 0xDB, 0xC3 }; // xchg bx, bx; ret

void move_memory(void* destination, const void* source, size_t count)
{
    __movsb(reinterpret_cast<uint8_t*>(destination), reinterpret_cast<const uint8_t*>(source), count);
}

class text_screen {
public:
    explicit text_screen() {
    }
    text_screen(const text_screen&) = delete;
    text_screen& operator=(const text_screen&) = delete;

    void clear() {
        for (int i = 0; i < height_; ++i) {
            clear_line(i);
        }
        x_ = 0;
        y_ = 0;
        set_cursor();
    }

    void put(int c) {
        if (c == '\r') {
            x_ = 0;
        } else if (c == '\n') {
            newline();
        } else {
            buffer_[x_ + y_ * width_] = static_cast<uint16_t>((attr_ << 8) | (c & 0xff));
            if (++x_ == width_) {
                newline();
            }
        }
        set_cursor();
    }

private:
    static constexpr uint16_t* buffer_ = reinterpret_cast<uint16_t*>(0xb8000);
    int         width_ = 80;
    int         height_ = 25;
    int         x_ = 0;
    int         y_ = 0;
    uint8_t     attr_ = 0x4F;

    void newline() {
        if (++y_ == height_) {
            y_ = height_-1;
            move_memory(&buffer_[0], &buffer_[width_], width_ * (height_ - 1) * sizeof(uint16_t));
            clear_line(y_);
        }
        x_ = 0;
    }

    void clear_line(int y) {
        for (int i = 0; i < width_; ++i) {
            buffer_[i + y * width_] = attr_ << 8;
        }
    }

    void set_cursor() {
        static constexpr uint16_t vga_crtc_index = 0x3D4;
        static constexpr uint16_t vga_crtc_data  = 0x3D5;
        enum vga_crtc_registers : uint8_t { // Ralf Browns's Port list (Table P0654)
            cursor_location_high = 0x0E, // R/W
            cursor_location_low  = 0x0F, // R/W
        };
        const auto cursor_location = static_cast<uint16_t>(x_ + y_ * width_);
        __outbyte(vga_crtc_index, cursor_location_high);
        __outbyte(vga_crtc_data,  cursor_location >> 8);
        __outbyte(vga_crtc_index, cursor_location_low);
        __outbyte(vga_crtc_data,  cursor_location & 0xff);
    }
};

void do_print_arg(text_screen& ts, const char* arg) {
    while (*arg) ts.put(*arg++);
}

template<typename I, typename = std::enable_if_t<std::is_integral_v<I>>>
void do_print_arg(text_screen& ts, I arg) {
    char buffer[64];
    int pos = sizeof(buffer);
    do {
        buffer[--pos] = (arg % 10) + '0';
        arg /= 10;
    } while (arg);
    for (; pos < sizeof(buffer); ++pos) {
        ts.put(buffer[pos]);
    }
}

void print(text_screen&) {}

template<typename T, typename... Ts>
void print(text_screen& ts, const T& arg, const Ts&... extra_args)
{
    do_print_arg(ts, arg);
    print(ts, extra_args...);
}

uint8_t read_key() {
    static constexpr uint8_t ps2_data_port    = 0x60;
    static constexpr uint8_t ps2_status_port  = 0x64; // when reading
    static constexpr uint8_t ps2_command_port = 0x64; // when writing
    static constexpr uint8_t ps2s_output_full = 0x01;
    static constexpr uint8_t ps2s_input_fill  = 0x02;

    while (!(__inbyte(ps2_status_port) & ps2s_output_full)) { // wait key
        __nop();
    }
    return __inbyte(ps2_data_port);  // read key
}

void small_exe()
{
    text_screen ts;
    ts.clear();
    for (int i = 0; i < 30; ++i) {
        print(ts, "Hello world! Line ", i, "\n");
    }

    print(ts, "Press any key to exit.\n");
    uint8_t c;
    do {
        c = read_key();
        print(ts, "Key pressed: ", c, "\n");
    } while (!c);
//    ((void (*)(void))(void*)bochs_magic_code)();
}

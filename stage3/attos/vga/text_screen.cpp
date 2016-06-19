#include "text_screen.h"
#include <attos/mem.h>
#include <intrin.h>

namespace attos { namespace vga {
// 0x0   Black
// 0x1   Blue
// 0x2   Green
// 0x3   Cyan
// 0x4   Red
// 0x5   Magenta
// 0x6   Brown
// 0x7   Light Gray
// 0x8   Gray
// 0x9   Light Blue
// 0xA   Light Green
// 0xB   Light Cyan
// 0xC   Light Red
// 0xD   Light Magenta
// 0xE   Light Yellow
// 0xF   White

constexpr auto screen_buffer = fixed_physical_address<uint16_t, 0xb8000>;
constexpr int  screen_width = 80;
constexpr int  screen_height = 25;

constexpr uint16_t crtc_index = 0x3D4;
constexpr uint16_t crtc_data  = 0x3D5;

enum class crtc_registers : uint8_t { // Ralf Browns's Port list (Table P0654)
    cursor_location_high = 0x0E, // R/W
    cursor_location_low  = 0x0F, // R/W
};

uint8_t get_register(crtc_registers reg)
{
    __outbyte(crtc_index, static_cast<uint8_t>(reg));
    return __inbyte(crtc_data);
}

void set_register(crtc_registers reg, uint8_t value)
{
    __outbyte(crtc_index, static_cast<uint8_t>(reg));
    __outbyte(crtc_data, value);
}

void cursor_location(uint16_t location)
{
    set_register(crtc_registers::cursor_location_high, location >> 8);
    set_register(crtc_registers::cursor_location_low, location & 0xff);
}

uint16_t cursor_location()
{
    return (get_register(crtc_registers::cursor_location_high) << 8) | get_register(crtc_registers::cursor_location_low);
}

text_screen::text_screen() {
    auto location = cursor_location();
    x_ = location % screen_width;
    y_ = location / screen_width;
}

void text_screen::clear() {
    for (int i = 0; i < screen_height; ++i) {
        clear_line(i);
    }
    x_ = 0;
    y_ = 0;
    set_cursor();
}

void text_screen::write(const void* data, size_t n) {
    auto p = reinterpret_cast<const uint8_t*>(data);
    while (n--) {
        put(*p++);
    }
    set_cursor();
}

void text_screen::put(int c) {
    if (c == '\r') {
        x_ = 0;
    } else if (c == '\n') {
        newline();
    } else {
        screen_buffer[x_ + y_ * screen_width] = static_cast<uint16_t>((attr_ << 8) | (c & 0xff));
        if (++x_ == screen_width) {
            newline();
        }
    }
}

void text_screen::newline() {
    if (++y_ == screen_height) {
        y_ = screen_height-1;
        move_memory(&screen_buffer[0], &screen_buffer[screen_width], screen_width * (screen_height - 1) * sizeof(uint16_t));
        clear_line(y_);
    }
    x_ = 0;
}

void text_screen::clear_line(int y) {
    for (int i = 0; i < screen_width; ++i) {
        screen_buffer[i + y * screen_width] = attr_ << 8;
    }
}

void text_screen::set_cursor() {
    cursor_location(static_cast<uint16_t>(x_ + y_ * screen_width));
}

} } // namespace attos::vga

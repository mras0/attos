#ifndef ATTOS_VGA_TEXT_SCREEN_H
#define ATTOS_VGA_TEXT_SCREEN_H

#include <attos/out_stream.h>

namespace attos { namespace vga {

class text_screen : public out_stream {
public:
    explicit text_screen();
    text_screen(const text_screen&) = delete;
    text_screen& operator=(const text_screen&) = delete;

    void clear();
    virtual void write(const void* data, size_t n) override;

private:
    static constexpr uint16_t* buffer_ = reinterpret_cast<uint16_t*>(0xb8000);
    int         width_ = 80;
    int         height_ = 25;
    int         x_ = 0;
    int         y_ = 0;
    uint8_t     attr_ = 0x07;

    void put(int c);
    void newline();
    void clear_line(int y);
    void set_cursor();
};

} } // namespace attos::vga

#endif

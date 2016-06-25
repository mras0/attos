#ifndef ATTOS_UTIL_H
#define ATTOS_UTIL_H

#define REQUIRE(expr) do { if (!(expr)) { ::attos::dbgout() << #expr << " failed in " << __FILE__ << ":" << __LINE__ << " . Hanging.\n"; ::attos::halt(); } } while (0);

namespace attos {

namespace detail {
static constexpr unsigned char bochs_magic_code[] = { 0x66, 0x87, 0xDB, 0xC3 }; // xchg bx, bx; ret
} // namespace detail
static auto bochs_magic = ((void (*)(void))(void*)detail::bochs_magic_code);

void halt();

class out_stream;

void set_dbgout(out_stream& os);
out_stream& dbgout();

template<typename T>
constexpr auto round_up(T val, T align)
{
    return val % align ? val + align - (val % align) : val;
}

}  // namespace attos

#endif

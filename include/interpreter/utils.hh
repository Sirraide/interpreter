#ifndef INTERPRETER_UTILS_HH
#define INTERPRETER_UTILS_HH

/// Make sure we don’t try to include this header from C.
#ifndef __cplusplus
#   error "This header is C++ only. Use <interpreter/utils.h> instead."
#endif

/// We only support little-endian for now.
#include <bit>
static_assert(std::endian::native == std::endian::little, "Only little-endian systems are supported.");

#include <fmt/format.h>

#define INTERP_CAT_(x, y) x##y
#define INTERP_CAT(x, y) INTERP_CAT_(x, y)

#define INTERP_STR_(x) #x
#define INTERP_STR(x) INTERP_STR_(x)

namespace interp {
inline namespace integers {
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using u128 = __uint128_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using i128 = __int128_t;
using f32 = float;
using f64 = double;
using usz = size_t;
using isz = ptrdiff_t;
} // namespace integers

template <typename ...arguments>
void die(fmt::format_string<arguments...> fmt, arguments&& ...args) {
    fmt::print(stderr, "Error: ");
    fmt::print(stderr, fmt, std::forward<arguments>(args)...);
    fmt::print(stderr, "\n");
    exit(1);
}

template <typename ...arguments>
void todo(fmt::format_string<arguments...> fmt = "", arguments&& ...args) {
    fmt::print(stderr, "TODO: ");
    if (fmt.get().size() != 0) fmt::print(stderr, fmt, std::forward<arguments>(args)...);
    else fmt::print (stderr, "Unimplemented");
    fmt::print(stderr, "\n");
    exit(42);
}

template <typename a, typename b>
constexpr inline bool is = std::is_same_v<std::remove_cvref_t<a>, std::remove_cvref_t<b>>;

/// Determine the width of a number.
constexpr inline usz number_width(usz base, usz value) {
    return not value ? 1 : usz(std::log(value) / std::log(base)) + 1;
}

}

#endif // INTERPRETER_UTILS_HH

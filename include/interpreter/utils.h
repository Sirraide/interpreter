#ifndef INTERPRETER_UTILS_H
#define INTERPRETER_UTILS_H

#include <stdint.h>
#include <stddef.h>

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

#define CAT_(x, y) x##y
#define CAT(x, y) CAT_(x, y)

#define STR_(x) #x
#define STR(x) STR_(x)

#endif // INTERPRETER_UTILS_H

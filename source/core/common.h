#pragma once
#include "glm/glm.hpp"
#include <cstdint>

using b8 = bool;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

inline constexpr glm::vec3 DEFAULT_RIGHT_VECTOR = glm::vec3(1.0f, 0.0f, 0.0f);
inline constexpr glm::vec3 DEFAULT_UP_VECTOR = glm::vec3(0.0f, 1.0f, 0.0f);
inline constexpr glm::vec3 DEFAULT_FORWARD_VECTOR = glm::vec3(0.0f, 0.0f, 1.0f);

#define KB(x) ((x) << 10ull)
#define MB(x) ((x) << 20ull)
#define GB(x) ((x) << 30ull)

#define ALIGN_UP_POW2(value, align) ((intptr_t)(value)+((align)-1) & (-(intptr_t)(align)))
#define ALIGN_DOWN_POW2(value, align) ((intptr_t)(value) & (-(intptr_t)(align)))
#define IS_POW2(value) (value != 0 && !(value & (value - 1)))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define IS_BIT_FLAG_SET(flags, bitflag) ((flags & bitflag) == bitflag)

#define PTR_OFFSET(ptr, offset) ((u8*)ptr + (offset))

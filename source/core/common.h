#pragma once
#include "glm/glm.hpp"

using b8 = bool;
using i8 = char;
using i16 = short;
using i32 = int;
using i64 = long long;
using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;
using f32 = float;
using f64 = double;

static constexpr glm::vec3 DEFAULT_RIGHT_VECTOR = glm::vec3(1.0f, 0.0f, 0.0f);
static constexpr glm::vec3 DEFAULT_UP_VECTOR = glm::vec3(0.0f, 1.0f, 0.0f);
static constexpr glm::vec3 DEFAULT_FORWARD_VECTOR = glm::vec3(0.0f, 0.0f, 1.0f);

#define KB(x) ((x) << 10ull)
#define MB(x) ((x) << 20ull)
#define GB(x) ((x) << 30ull)

#define ALIGN_UP_POW2(value, align) ((intptr_t)(value)+((align)-1) & (-(intptr_t)(align)))
#define ALIGN_DOWN_POW2(value, align) ((intptr_t)(value) & (-(intptr_t)(align)))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define IS_BIT_FLAG_SET(flags, bitflag) ((flags & bitflag) == bitflag)

#define PTR_OFFSET(ptr, offset) ((u8*)ptr + offset)

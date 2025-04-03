#pragma once
#include <cstdint>
#include "glm/glm.hpp"

inline constexpr glm::vec3 DEFAULT_RIGHT_VECTOR = glm::vec3(1.0f, 0.0f, 0.0f);
inline constexpr glm::vec3 DEFAULT_UP_VECTOR = glm::vec3(0.0f, 1.0f, 0.0f);
inline constexpr glm::vec3 DEFAULT_FORWARD_VECTOR = glm::vec3(0.0f, 0.0f, 1.0f);

#define KB(x) ((x) << 10ull)
#define MB(x) ((x) << 20ull)
#define GB(x) ((x) << 30ull)

#define TO_KB(x) ((x) >> 10ull)
#define TO_MB(x) ((x) >> 20ull)
#define TO_GB(x) ((x) >> 30ull)

#define ALIGN_UP_POW2(value, align) ((intptr_t)(value)+((align)-1) & (-(intptr_t)(align)))
#define ALIGN_DOWN_POW2(value, align) ((intptr_t)(value) & (-(intptr_t)(align)))
#define IS_POW2(value) (value != 0 && !(value & (value - 1)))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define IS_BIT_FLAG_SET(flags, bitflag) ((flags & bitflag) == bitflag)

#define PTR_OFFSET(ptr, offset) ((uint8_t*)ptr + (offset))

#pragma once
#include <stdint.h>

static constexpr glm::vec3 DEFAULT_RIGHT_VECTOR = glm::vec3(1.0f, 0.0f, 0.0f);
static constexpr glm::vec3 DEFAULT_UP_VECTOR = glm::vec3(0.0f, 1.0f, 0.0f);
static constexpr glm::vec3 DEFAULT_FORWARD_VECTOR = glm::vec3(0.0f, 0.0f, 1.0f);

template<typename T>
inline intptr_t AlignUp(T x, uint32_t align)
{
	return ((intptr_t)(x)+((align)-1) & (-(intptr_t)(align)));
}

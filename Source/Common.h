#pragma once
#include <stdint.h>

template<typename T>
inline intptr_t AlignUp(T x, uint32_t align)
{
	return ((intptr_t)(x)+((align)-1) & (-(intptr_t)(align)));
}

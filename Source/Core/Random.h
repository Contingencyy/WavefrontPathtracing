#pragma once

namespace Random
{

	static uint32_t s_seed = 0xC977E154;

	inline uint32_t WangHash(uint32_t seed)
	{
		seed = (seed ^ 61) ^ (seed >> 16);
		seed *= 9, seed = seed ^ (seed >> 4);
		seed *= 0x27d4eb2d;
		seed = seed ^ (seed >> 15);
		return seed;
	}

	inline uint32_t UInt32()
	{
		s_seed ^= s_seed << 13;
		s_seed ^= s_seed >> 17;
		s_seed ^= s_seed << 5;
		return s_seed;
	}

	inline uint32_t UInt32(uint32_t& customSeed)
	{
		customSeed ^= customSeed << 13;
		customSeed ^= customSeed >> 17;
		customSeed ^= customSeed << 5;
		return customSeed;
	}

	inline uint32_t UInt32Range(uint32_t min, uint32_t max)
	{
		if (max - min == 0)
			return min;
		return min + (UInt32() % ((max + 1) - min));
	}

	inline float Float()
	{
		return UInt32() * 2.3283064365387e-10f;
	}

	inline float FloatRange(float min = 0.0f, float max = 1.0f)
	{
		return min + (Float() * (max - min));
	}

}

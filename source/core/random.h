#pragma once

namespace random
{

	static uint32_t s_seed = 0xC977E154;

	inline uint32_t wanghash(uint32_t Seed)
	{
		Seed = (Seed ^ 61) ^ (Seed >> 16);
		Seed *= 9, Seed = Seed ^ (Seed >> 4);
		Seed *= 0x27d4eb2d;
		Seed = Seed ^ (Seed >> 15);
		return Seed;
	}

	inline uint32_t rand_uint32_t()
	{
		s_seed ^= s_seed << 13;
		s_seed ^= s_seed >> 17;
		s_seed ^= s_seed << 5;
		return s_seed;
	}

	inline uint32_t rand_uint32_t(uint32_t& custom_seed)
	{
		custom_seed ^= custom_seed << 13;
		custom_seed ^= custom_seed >> 17;
		custom_seed ^= custom_seed << 5;
		return custom_seed;
	}

	inline uint32_t rand_uint32_t_range(uint32_t min, uint32_t max)
	{
		if (max - min == 0)
			return min;
		return min + (rand_uint32_t() % ((max + 1) - min));
	}

	// Range 0..1
	inline float rand_float()
	{
		return rand_uint32_t() * 2.3283064365387e-10f;
	}

	inline float rand_float_range(float min = 0.0f, float max = 1.0f)
	{
		return min + (float() * (max - min));
	}

}

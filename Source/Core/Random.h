#pragma once

namespace random
{

	static u32 s_seed = 0xC977E154;

	inline u32 wanghash(u32 Seed)
	{
		Seed = (Seed ^ 61) ^ (Seed >> 16);
		Seed *= 9, Seed = Seed ^ (Seed >> 4);
		Seed *= 0x27d4eb2d;
		Seed = Seed ^ (Seed >> 15);
		return Seed;
	}

	inline u32 rand_u32()
	{
		s_seed ^= s_seed << 13;
		s_seed ^= s_seed >> 17;
		s_seed ^= s_seed << 5;
		return s_seed;
	}

	inline u32 rand_u32(u32& custom_seed)
	{
		custom_seed ^= custom_seed << 13;
		custom_seed ^= custom_seed >> 17;
		custom_seed ^= custom_seed << 5;
		return custom_seed;
	}

	inline u32 rand_u32_range(u32 min, u32 max)
	{
		if (max - min == 0)
			return min;
		return min + (rand_u32() % ((max + 1) - min));
	}

	// Range 0..1
	inline f32 rand_float()
	{
		return rand_u32() * 2.3283064365387e-10f;
	}

	inline f32 rand_float_range(f32 min = 0.0f, f32 max = 1.0f)
	{
		return min + (f32() * (max - min));
	}

}

#pragma once

namespace Random
{

	static u32 s_Seed = 0xC977E154;

	inline u32 WangHash(u32 Seed)
	{
		Seed = (Seed ^ 61) ^ (Seed >> 16);
		Seed *= 9, Seed = Seed ^ (Seed >> 4);
		Seed *= 0x27d4eb2d;
		Seed = Seed ^ (Seed >> 15);
		return Seed;
	}

	inline u32 UInt32()
	{
		s_Seed ^= s_Seed << 13;
		s_Seed ^= s_Seed >> 17;
		s_Seed ^= s_Seed << 5;
		return s_Seed;
	}

	inline u32 UInt32(u32& CustomSeed)
	{
		CustomSeed ^= CustomSeed << 13;
		CustomSeed ^= CustomSeed >> 17;
		CustomSeed ^= CustomSeed << 5;
		return CustomSeed;
	}

	inline u32 UInt32Range(u32 Min, u32 Max)
	{
		if (Max - Min == 0)
			return Min;
		return Min + (UInt32() % ((Max + 1) - Min));
	}

	// Range 0..1
	inline f32 Float()
	{
		return UInt32() * 2.3283064365387e-10f;
	}

	inline f32 FloatRange(f32 Min = 0.0f, f32 Max = 1.0f)
	{
		return Min + (f32() * (Max - Min));
	}

}

#pragma once
#include "Core/Common.h"

namespace Hash
{

	static inline u32 RT_Rotl32(u32 x, i8 r)
	{
		return (x << r) | (x >> (32 - r));
	}

	static inline u32 RT_FMix(u32 h)
	{
		h ^= h >> 16; h *= 0x85ebca6b;
		h ^= h >> 13; h *= 0xc2b2ae35;
		return h ^= h >> 16;
	}

	static u32 DJB2(const void* in)
	{
		u8* data = (u8*)in;
		u32 hash = 5381;
		int c;

		while (c = *data++)
		{
			hash = ((hash << 5) + hash) + c;
		}

		return hash;
	}

	static u32 Murmur2_32(const void* in, u32 len, u32 seed)
	{
		const u32 m = 0x5bd1e995;
		const int r = 24;

		u8* data = (u8*)in;
		u32 hash = seed ^ len;

		while (len >= 4)
		{
			u32 k = *(u32*)data;

			k *= m;
			k ^= k >> r;
			k *= m;

			hash *= m;
			hash ^= k;

			data += 4;
			len -= 4;
		}

		switch (len)
		{
		case 3: hash ^= data[2] << 16;
		case 2: hash ^= data[1] << 8;
		case 1: hash ^= data[0];
			hash *= m;
		}

		hash ^= hash >> 13;
		hash *= m;
		hash ^= hash >> 15;

		return hash;
	}

	static u32 Murmur3_32(const void* in, u32 len, u32 seed)
	{
		const u8* tail = (const u8*)in + (len / 4) * 4;
		u32 c1 = 0xcc9e2d51, c2 = 0x1b873593;

		for (u32* p = (u32*)in; p < (const u32*)tail; p++)
		{
			u32 k1 = *p; k1 *= c1; k1 = RT_Rotl32(k1, 15); k1 *= c2; // MUR1
			seed ^= k1; seed = RT_Rotl32(seed, 13); seed = seed * 5 + 0xe6546b64; // MUR2
		}

		u32 t = 0; // handle up to 3 tail bytes
		switch (len & 3)
		{
		case 3: t ^= tail[2] << 16;
		case 2: t ^= tail[1] << 8;
		case 1: { t ^= tail[0]; t *= c1; t = RT_Rotl32(t, 15); t *= c2; seed ^= t; };
		}
		return RT_FMix(seed ^ len);
	}

}

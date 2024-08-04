#pragma once
#include "Renderer/RaytracingTypes.h"

struct Ray;
class BVHInstance;

class TLAS
{
public:
	void Build(MemoryArena* Arena, BVHInstance* BLAS, u32 BLASCount);
	void TraceRay(Ray& Ray, HitResult& HitResult) const;

private:
	struct TLASNode
	{
		// LeftRight will store two separate indices, 16 bit for each
		union { struct { glm::vec3 AabbMin; u32 LeftRight; }; __m128 AabbMin4 = {}; };
		union { struct { glm::vec3 AabbMax; u32 BlasInstanceIdx; }; __m128 AabbMax4 = {}; };

		b8 IsLeafNode() const;
	};

private:
	u32 FindBestMatch(u32 A, const u32* Indices, u32 IndexCount);

private:
	u32 m_TLASNodeCount;
	TLASNode* m_TLASNodes;

	u32 m_BLASInstanceCount;
	BVHInstance* m_BLASInstances;
	
	u32 m_NodeAt = 0;

};

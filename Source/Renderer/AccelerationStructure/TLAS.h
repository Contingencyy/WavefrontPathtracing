#pragma once

struct Ray;
class BVH;

class TLAS
{
public:
	struct HitResult
	{
		uint32_t instanceIndex = ~0u;
		uint32_t primitiveIndex = ~0u;
	};

public:
	TLAS() = default;

	void Build(const std::vector<BVH>& blas);
	void TraceRay(Ray& ray);

private:
	struct TLASNode
	{
		union { struct { glm::vec3 aabbMin; uint32_t leftBLAS; }; __m128 aabbMin4 = {}; };
		union { struct { glm::vec3 aabbMax; bool isLeaf; }; __m128 aabbMax4 = {}; };
	};

private:
	std::vector<TLASNode> m_TLASNodes;
	std::vector<BVH> m_BLAS;
	
	uint32_t m_CurrentNodeIndex = 0;

};

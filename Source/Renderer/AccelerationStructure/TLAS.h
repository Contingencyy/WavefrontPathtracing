#pragma once

struct Ray;
class BVH;

class TLAS
{
public:
	struct TraceResult
	{
		uint32_t instanceIndex = ~0u;
		uint32_t primitiveIndex = ~0u;
	};

public:
	TLAS() = default;

	void Build(const std::vector<BVH>& blas);
	void TraceRay(Ray& ray) const;

private:
	struct TLASNode
	{
		// leftRight will store two separate indices, 16 bit for each
		union { struct { glm::vec3 aabbMin; uint32_t leftRight; }; __m128 aabbMin4 = {}; };
		union { struct { glm::vec3 aabbMax; uint32_t blasIndex; }; __m128 aabbMax4 = {}; };

		bool IsLeafNode() const;
	};

private:
	uint32_t FindBestMatch(uint32_t A, const std::span<uint32_t>& indices);

private:
	std::vector<TLASNode> m_TLASNodes;
	std::vector<BVH> m_BLAS;
	
	uint32_t m_CurrentNodeIndex = 0;

};

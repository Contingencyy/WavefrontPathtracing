#pragma once

struct Ray;
class BVHInstance;

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

	void Build(const std::vector<BVHInstance>& blas);
	TraceResult TraceRay(Ray& ray) const;

	glm::vec3 GetNormal(uint32_t instanceIndex, uint32_t primitiveIndex) const;

private:
	struct TLASNode
	{
		// leftRight will store two separate indices, 16 bit for each
		union { struct { glm::vec3 aabbMin; uint32_t leftRight; }; __m128 aabbMin4 = {}; };
		union { struct { glm::vec3 aabbMax; uint32_t blasInstanceIndex; }; __m128 aabbMax4 = {}; };

		bool IsLeafNode() const;
	};

private:
	uint32_t FindBestMatch(uint32_t A, const std::span<uint32_t>& indices);

private:
	std::vector<TLASNode> m_TLASNodes;
	std::vector<BVHInstance> m_BLASInstances;
	
	uint32_t m_CurrentNodeIndex = 0;

};

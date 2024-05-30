#pragma once

static constexpr float PI = 3.14159265f;
static constexpr float TWO_PI = 2.0f * PI;
static constexpr float INV_PI = 1.0f / PI;
static constexpr float INV_TWO_PI = 1.0f / TWO_PI;

static constexpr float RAY_MAX_T = FLT_MAX;
static constexpr float RAY_NUDGE_MODIFIER = 0.001f;
static constexpr uint32_t RAY_MAX_RECURSION_DEPTH = 3;

struct Material
{
	glm::vec3 albedo = {};
	float specular = 0.0f;
	float refractivity = 0.0f;

	bool isEmissive = false;
	glm::vec3 emissive = {};
	float intensity = 0.0f;
};

struct HitSurfaceData
{
	uint32_t objIdx = ~0u;
	Material objMat = {};

	glm::vec3 pos = {};
	glm::vec3 normal = {};
};

struct Ray
{
	glm::vec3 origin = {};
	glm::vec3 dir = {};
	glm::vec3 invDir = {};
	float t = RAY_MAX_T;

	Ray(const glm::vec3& orig, const glm::vec3& dir)
		: origin(orig), dir(dir)
	{
		invDir = 1.0f / dir;
	}
};

struct Triangle
{
	glm::vec3 p0 = {};
	glm::vec3 p1 = {};
	glm::vec3 p2 = {};
};

struct Sphere
{
	glm::vec3 center = {};
	float radiusSquared = 1.0f;
};

struct Plane
{
	glm::vec3 point = {};
	glm::vec3 normal = {};
};

struct AABB
{
	glm::vec3 pmin = {};
	glm::vec3 pmax = {};
};

enum PrimitiveType
{
	PrimitiveType_Triangle,
	PrimitiveType_Sphere,
	PrimitiveType_Plane,
	PrimitiveType_AABB
};

struct Primitive
{
	PrimitiveType type;

	union
	{
		Triangle tri;
		Sphere sphere;
		Plane plane;
		AABB aabb;
	};
};

#pragma once

static constexpr float PI = 3.14159265f;
static constexpr float TWO_PI = 2.0f * PI;
static constexpr float INV_PI = 1.0f / PI;
static constexpr float INV_TWO_PI = 1.0f / TWO_PI;

static constexpr float RAY_MAX_T = FLT_MAX;
static constexpr float RAY_NUDGE_MODIFIER = 0.001f;

struct Material
{
	glm::vec3 albedo = {};
	float specular = 0.0f;

	float refractivity = 0.0f;
	float ior = 1.0f;
	glm::vec3 absorption = {};

	bool isEmissive = false;
	glm::vec3 emissive = {};
	float intensity = 0.0f;

	static Material MakeDiffuse(const glm::vec3& albedo)
	{
		Material mat = {};
		mat.albedo = albedo;

		return mat;
	}

	static Material MakeSpecular(const glm::vec3& albedo, float spec)
	{
		Material mat = {};
		mat.albedo = albedo;
		mat.specular = spec;

		return mat;
	}

	static Material MakeRefractive(const glm::vec3& albedo, float spec, float refract, float ior, const glm::vec3& absorption)
	{
		Material mat = {};
		mat.albedo = albedo;
		mat.specular = spec;
		mat.refractivity = refract;
		mat.ior = ior;
		mat.absorption = absorption;

		return mat;
	}

	static Material MakeEmissive(const glm::vec3& emissive, float intensity)
	{
		Material mat = {};
		mat.isEmissive = true;
		mat.emissive = emissive;
		mat.intensity = intensity;

		return mat;
	}
};

struct Ray
{
	union { struct { glm::vec3 origin; float dummy; }; __m128 origin4 = {}; };
	union { struct { glm::vec3 dir; float dummy; }; __m128 dir4 = {}; };
	union { struct { glm::vec3 invDir; float dummy; }; __m128 invDir4 = {}; };
	float t = RAY_MAX_T;
	uint32_t bvhDepth = 0;

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

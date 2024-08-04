#pragma once

static constexpr f32 PI = 3.14159265f;
static constexpr f32 TWO_PI = 2.0f * PI;
static constexpr f32 INV_PI = 1.0f / PI;
static constexpr f32 INV_TWO_PI = 1.0f / TWO_PI;
static constexpr glm::vec2 INV_ATAN = glm::vec2(0.1591f, 0.3183f);

static constexpr f32 RAY_MAX_T = FLT_MAX;
static constexpr f32 RAY_NUDGE_MODIFIER = 0.001f;

static constexpr u32 INSTANCE_IDX_INVALID = ~0u;
static constexpr u32 PRIM_IDX_INVALID = ~0u;

struct HitResult
{
	glm::vec3 Pos = glm::vec3(0.0f);
	glm::vec3 Normal = glm::vec3(0.0f);
	glm::vec3 Bary = glm::vec3(0.0f);
	f32 t = FLT_MAX;

	u32 InstanceIdx = INSTANCE_IDX_INVALID;
	u32 PrimIdx = PRIM_IDX_INVALID;

	b8 HasHitGeometry() const
	{
		return (InstanceIdx != INSTANCE_IDX_INVALID && PrimIdx != PRIM_IDX_INVALID);
	}
};

struct Material
{
	glm::vec3 Albedo = {};
	f32 Specular = 0.0f;

	f32 Refractivity = 0.0f;
	f32 Ior = 1.0f;
	glm::vec3 Absorption = {};

	b8 bEmissive = false;
	glm::vec3 Emissive = {};
	f32 Intensity = 0.0f;

	static Material MakeDiffuse(const glm::vec3& Albedo)
	{
		Material Mat = {};
		Mat.Albedo = Albedo;

		return Mat;
	}

	static Material MakeSpecular(const glm::vec3& Albedo, f32 Spec)
	{
		Material Mat = {};
		Mat.Albedo = Albedo;
		Mat.Specular = Spec;

		return Mat;
	}

	static Material MakeRefractive(const glm::vec3& Albedo, f32 Spec, f32 Refract, f32 Ior, const glm::vec3& Absorption)
	{
		Material Mat = {};
		Mat.Albedo = Albedo;
		Mat.Specular = Spec;
		Mat.Refractivity = Refract;
		Mat.Ior = Ior;
		Mat.Absorption = Absorption;

		return Mat;
	}

	static Material MakeEmissive(const glm::vec3& Emissive, f32 Intensity)
	{
		Material Mat = {};
		Mat.bEmissive = true;
		Mat.Emissive = Emissive;
		Mat.Intensity = Intensity;

		return Mat;
	}
};

struct Ray
{
	union { struct { glm::vec3 Origin; f32 Dummy; }; __m128 Origin4 = {}; };
	union { struct { glm::vec3 Dir; f32 Dummy; }; __m128 Dir4 = {}; };
	union { struct { glm::vec3 InvDir; f32 Dummy; }; __m128 InvDir4 = {}; };

	f32 t = RAY_MAX_T;
	u32 BvhDepth = 0;

	Ray() = default;
};

inline Ray MakeRay(const glm::vec3& Origin, const glm::vec3& Dir)
{
	Ray Ray = {};
	Ray.Origin = Origin;
	Ray.Dir = Dir;
	Ray.InvDir = 1.0f / Dir;

	return Ray;
}

struct Triangle
{
	glm::vec3 p0 = {};
	glm::vec3 p1 = {};
	glm::vec3 p2 = {};

	glm::vec3 n0 = {};
	glm::vec3 n1 = {};
	glm::vec3 n2 = {};
};

struct Sphere
{
	glm::vec3 Center = {};
	f32 RadiusSquared = 1.0f;
};

struct Plane
{
	glm::vec3 Point = {};
	glm::vec3 Normal = {};
};

struct AABB
{
	glm::vec3 PMin = {};
	glm::vec3 PMax = {};
};

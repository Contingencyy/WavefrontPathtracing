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

struct hit_result_t
{
	glm::vec3 bary = glm::vec3(0.0f);
	f32 t = FLT_MAX;

	u32 instance_idx = INSTANCE_IDX_INVALID;
	u32 prim_idx = PRIM_IDX_INVALID;

	b8 has_hit_geometry() const
	{
		return (instance_idx != INSTANCE_IDX_INVALID && prim_idx != PRIM_IDX_INVALID);
	}
};

struct ray_t
{
	union { struct { glm::vec3 origin; f32 dummy; }; __m128 origin4 = {}; };
	union { struct { glm::vec3 dir; f32 dummy; }; __m128 dir4 = {}; };
	union { struct { glm::vec3 inv_dir; f32 dummy; }; __m128 inv_dir4 = {}; };

	f32 t = RAY_MAX_T;
	u32 bvh_depth = 0;

	ray_t() = default;
};

inline ray_t make_ray(const glm::vec3& origin, const glm::vec3& dir)
{
	ray_t ray = {};
	ray.origin = origin;
	ray.dir = dir;
	ray.inv_dir = 1.0f / dir;

	return ray;
}

struct triangle_t
{
	glm::vec3 p0 = {};
	glm::vec3 p1 = {};
	glm::vec3 p2 = {};

	glm::vec3 n0 = {};
	glm::vec3 n1 = {};
	glm::vec3 n2 = {};
};

struct sphere_t
{
	glm::vec3 Center = {};
	f32 RadiusSquared = 1.0f;
};

struct plane_t
{
	glm::vec3 Point = {};
	glm::vec3 normal = {};
};

struct aabb_t
{
	glm::vec3 pmin = {};
	glm::vec3 pmax = {};
};

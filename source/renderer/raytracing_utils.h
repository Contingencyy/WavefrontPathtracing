#pragma once
#include "raytracing_types.h"
#include "core/camera/camera.h"
#include "core/random.h"

namespace rt_util
{

	/*
		INTERSECTION FUNCTIONS
	*/

	// Möller-Trumbore
	inline b8 intersect(const triangle_t& triangle, ray_t& ray, f32& out_t, glm::vec3& out_bary)
	{
		// This algorithm is very sensitive to eps values, so we need a very small one, otherwise transforming/scaling the ray breaks this
		const f32 eps = 0.00000000001f;

		// First check whether we hit the plane the triangle is on
		glm::vec3 edge1 = triangle.p1 - triangle.p0;
		glm::vec3 edge2 = triangle.p2 - triangle.p0;

		glm::vec3 h = glm::cross(ray.dir, edge2);
		f32 det = glm::dot(edge1, h);

		if (glm::abs(det) < eps)
			return false;

		// Then check whether the ray-plane intersection lies outside of the triangle using barycentric coordinates
		f32 f = 1.0f / det;
		glm::vec3 s = ray.origin - triangle.p0;
		f32 v = f * glm::dot(s, h);

		if (v < 0.0f || v > 1.0f)
			return false;

		glm::vec3 q = glm::cross(s, edge1);
		f32 w = f * glm::dot(ray.dir, q);

		if (w < 0.0f || v + w > 1.0f)
			return false;

		// ray-plane intersection is inside the triangle, so we can compute "t" now
		f32 t = f * glm::dot(edge2, q);

		if (t < eps || t >= ray.t)
			return false;

		ray.t = t;
		out_t = t;
		out_bary = glm::vec3(1.0f - v - w, v, w);

		return true;
	}

	inline b8 intersect(const sphere_t& sphere, ray_t& ray)
	{
		f32 t0 = 0.0f, t1 = 0.0f;
		glm::vec3 L = sphere.Center - ray.origin;
		f32 tca = glm::dot(L, ray.dir);

		if (tca < 0.0f)
			return false;

		f32 d2 = glm::dot(L, L) - tca * tca;

		if (d2 > sphere.RadiusSquared)
			return false;

		f32 thc = std::sqrtf(sphere.RadiusSquared - d2);
		t0 = tca - thc;
		t1 = tca + thc;

		if (t0 > t1)
			std::swap(t0, t1);

		if (t0 < 0.0f)
		{
			t0 = t1;

			if (t0 < 0.0f)
				return false;
		}

		if (t0 < ray.t)
		{
			ray.t = t0;
			return true;
		}

		return false;
	}

	inline b8 intersect(const plane_t& plane, ray_t& ray)
	{
		f32 det = glm::dot(ray.dir, plane.normal);

		if (glm::abs(det) > 1e-6)
		{
			glm::vec3 p0 = plane.Point - ray.origin;
			f32 t = glm::dot(p0, plane.normal) / det;

			if (t > 0.0f && t < ray.t)
			{
				ray.t = t;
				return true;
			}
		}

		return false;
	}

	inline b8 intersect(const aabb_t& aabb, ray_t& ray)
	{
		f32 tx1 = (aabb.pmin.x - ray.origin.x) * ray.inv_dir.x, tx2 = (aabb.pmax.x - ray.origin.x) * ray.inv_dir.x;
		f32 tmin = MIN(tx1, tx2), tmax = MAX(tx1, tx2);
		f32 ty1 = (aabb.pmin.y - ray.origin.y) * ray.inv_dir.y, ty2 = (aabb.pmax.y - ray.origin.y) * ray.inv_dir.y;
		tmin = MAX(tmin, MIN(ty1, ty2)), tmax = MIN(tmax, MAX(ty1, ty2));
		f32 tz1 = (aabb.pmin.z - ray.origin.z) * ray.inv_dir.z, tz2 = (aabb.pmax.z - ray.origin.z) * ray.inv_dir.z;
		tmin = MAX(tmin, MIN(tz1, tz2)), tmax = MIN(tmax, MAX(tz1, tz2));

		if (tmax >= tmin && tmin < ray.t && tmax > 0.0f)
		{
			ray.t = tmin;
			return true;
		}

		return false;
	}

	inline f32 intersect_sse(const __m128 aabb_min, const __m128 aabb_max, const ray_t& ray)
	{
		__m128 mask4 = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_set_ps(1, 0, 0, 0));

		__m128 t1 = _mm_mul_ps(_mm_sub_ps(_mm_and_ps(aabb_min, mask4), ray.origin4), ray.inv_dir4);
		__m128 t2 = _mm_mul_ps(_mm_sub_ps(_mm_and_ps(aabb_max, mask4), ray.origin4), ray.inv_dir4);
		__m128 vmax4 = _mm_max_ps(t1, t2), vmin4 = _mm_min_ps(t1, t2);

		f32 tmax = MIN(vmax4.m128_f32[0], MIN(vmax4.m128_f32[1], vmax4.m128_f32[2]));
		f32 tmin = MAX(vmin4.m128_f32[0], MAX(vmin4.m128_f32[1], vmin4.m128_f32[2]));

		if (tmax >= tmin && tmin < ray.t && tmax > 0.0f)
		{
			return tmin;
		}

		return FLT_MAX;
	}

	/*
		HIT SURFACE
	*/

	inline glm::vec3 get_hit_normal(const triangle_t& triangle, const glm::vec3& bary)
	{
		return glm::normalize(glm::vec3(bary.x * triangle.n0 + bary.y * triangle.n1 + bary.z * triangle.n2));
	}

	inline glm::vec3 get_hit_normal(const sphere_t& sphere, const glm::vec3& hit_pos)
	{
		return glm::normalize(hit_pos - sphere.Center);
	}

	inline glm::vec3 get_hit_normal(const plane_t& plane)
	{
		return plane.normal;
	}

	inline glm::vec3 get_hit_normal(const aabb_t& Aabb, const glm::vec3& hit_pos)
	{
		const glm::vec3 center = (Aabb.pmax + Aabb.pmin) * 0.5f;
		const glm::vec3 half_size = (Aabb.pmax - Aabb.pmin) * 0.5f;
		const glm::vec3 center_to_hit = hit_pos - center;

		return glm::normalize(glm::sign(center_to_hit) * glm::step(-RAY_NUDGE_MODIFIER, glm::abs(center_to_hit) - half_size));
	}

	/*
		PRIMITIVES
	*/

	inline glm::vec3 get_triangle_centroid(const triangle_t& triangle)
	{
		return (triangle.p0 + triangle.p1 + triangle.p2) * 0.3333f;
	}

	inline void get_triangle_min_max(const triangle_t& triangle, glm::vec3& out_min, glm::vec3& out_max)
	{
		out_min = glm::min(triangle.p0, triangle.p1);
		out_max = glm::max(triangle.p0, triangle.p1);

		out_min = glm::min(out_min, triangle.p2);
		out_max = glm::max(out_max, triangle.p2);
	}

	inline f32 get_aabb_volume(const glm::vec3& aabb_min, const glm::vec3& aabb_max)
	{
		const glm::vec3 extent = aabb_max - aabb_min;
		return extent.x * extent.y + extent.y * extent.z + extent.z * extent.x;
	}

	inline void grow_aabb(glm::vec3& aabb_min, glm::vec3& aabb_max, const glm::vec3& pos)
	{
		aabb_min = glm::min(aabb_min, pos);
		aabb_max = glm::max(aabb_max, pos);
	}

	inline void grow_aabb(glm::vec3& aabb_min, glm::vec3& aabb_max, const glm::vec3& other_min, const glm::vec3& other_max)
	{
		aabb_min = glm::min(aabb_min, other_min);
		aabb_max = glm::max(aabb_max, other_max);
	}

	/*
		CAMERA
	*/

	inline ray_t construct_camera_ray(const camera_t& camera, u32 pixel_x, u32 pixel_y, f32 tan_fov,
		f32 aspect, f32 inv_screen_width, f32 inv_screen_height)
	{
		// Calculate UV of the pixel coordinate on the screen plane, 0..1 range,
		// and apply a 0.5 offset so that we get the center of the pixel and not its top-left corner
		// Note that unlike in rasterization, the NDC coordinates range from 0..1 instead of -1..1
		f32 u = (pixel_x + 0.5f) * inv_screen_width;
		f32 v = (pixel_y + 0.5f) * inv_screen_height;

		// Remap pixel positions from 0..1 to -1..1
		glm::vec2 pixel_pos_view = glm::vec2(2.0f * u - 1.0f, 1.0f - 2.0f * v);
		// Apply aspect ratio by scaling the pixel X coordinate in NDC space and FOV
		pixel_pos_view.x *= aspect;
		pixel_pos_view.x *= tan_fov;
		pixel_pos_view.y *= tan_fov;

		// transform_mat pixel view-space position and direction to world-space
		glm::vec3 camera_to_pixel_world = glm::vec3(camera.transform_mat * glm::vec4(pixel_pos_view.x, pixel_pos_view.y, 1.0f, 0.0f));
		glm::vec3 camera_origin_world = glm::vec3(camera.transform_mat * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		camera_to_pixel_world = glm::normalize(camera_to_pixel_world);

		return make_ray(camera_origin_world, camera_to_pixel_world);
	}

	/*
		SAMPLING
	*/

	inline void create_orthonormal_basis(const glm::vec3& normal, glm::vec3& tangent, glm::vec3& bitangent)
	{
		if (glm::abs(normal.x) > glm::abs(normal.z))
			tangent = glm::normalize(glm::vec3(-normal.y, normal.x, 0.0f));
		else
			tangent = glm::normalize(glm::vec3(0.0f, -normal.z, normal.y));

		bitangent = glm::cross(normal, tangent);
	}

	inline glm::vec3 direction_to_normal_space(const glm::vec3& normal, const glm::vec3& sample_dir)
	{
		glm::vec3 tangent, bitangent;
		create_orthonormal_basis(normal, tangent, bitangent);

		return glm::vec3(sample_dir.x * tangent + sample_dir.y * normal + sample_dir.z * bitangent);
	}

	inline glm::vec3 uniform_hemisphere_sample(const glm::vec3& normal)
	{
		f32 r1 = random::rand_float();
		f32 r2 = random::rand_float();

		f32 sin_theta = glm::sqrt(1.0f - r1 * r1);
		f32 phi = 2.0f * PI * r2;

		return direction_to_normal_space(normal, glm::vec3(sin_theta * glm::cos(phi), r1, sin_theta * glm::sin(phi)));
	}

	inline glm::vec3 cosine_weighted_hemisphere_sample(const glm::vec3& normal)
	{
		f32 r1 = random::rand_float();
		f32 r2 = random::rand_float();

		f32 cos_theta = glm::sqrt(r1);
		f32 sin_theta = glm::sqrt(1.0f - cos_theta * cos_theta);
		f32 phi = 2.0f * PI * r2;

		return direction_to_normal_space(normal, glm::vec3(sin_theta * glm::cos(phi), cos_theta, sin_theta * glm::sin(phi)));
	}

	inline glm::vec3 reflect(const glm::vec3& in_dir, const glm::vec3& normal)
	{
		return in_dir - 2.0f * normal * glm::dot(in_dir, normal);
	}

	inline glm::vec3 refract(const glm::vec3& dir, const glm::vec3& normal, f32 eta, f32 cosi, f32 k)
	{
		return glm::normalize(dir * eta + ((eta * cosi - glm::sqrt(k)) * normal));
	}

	inline f32 fresnel(f32 in, f32 out, f32 ior_outside, f32 ior_inside)
	{
		f32 sPolarized = (ior_outside * in - ior_inside * out) /
			(ior_outside * in + ior_inside * out);
		f32 pPolarized = (ior_outside * out - ior_inside * in) /
			(ior_outside * out + ior_inside * in);
		return 0.5f * ((sPolarized * sPolarized) + (pPolarized * pPolarized));
	}

	inline f32 survival_probability_rr(const glm::vec3& albedo)
	{
		return glm::clamp(glm::max(glm::max(albedo.x, albedo.y), albedo.z), 0.0f, 1.0f);
	}

	inline glm::vec2 direction_to_equirect_uv(const glm::vec3& dir)
	{
		glm::vec2 uv = glm::vec2(glm::atan(dir.z, dir.x), glm::asin(dir.y));
		uv *= INV_ATAN;
		uv += 0.5f;

		return uv;
	}

	/*
		COLOR
	*/

	inline u32 vec4_to_u32(const glm::vec4& rgba)
	{
		u8 r = static_cast<u8>(255.0f * MIN(1.0f, rgba.r));
		u8 g = static_cast<u8>(255.0f * MIN(1.0f, rgba.g));
		u8 b = static_cast<u8>(255.0f * MIN(1.0f, rgba.b));
		u8 a = static_cast<u8>(255.0f * MIN(1.0f, rgba.a));
		return (a << 24) | (b << 16) | (g << 8) | r;
	}

	inline glm::vec3 linear_to_srgb(const glm::vec3& linear)
	{
		glm::vec3 clamped = glm::clamp(linear, 0.0f, 1.0f);

		glm::bvec3 cutoff = glm::lessThan(clamped, glm::vec3(0.0031308f));
		glm::vec3 higher = glm::vec3(1.055f) * glm::pow(clamped, glm::vec3(1.0f / 2.4f)) - glm::vec3(0.055f);
		glm::vec3 lower = clamped * glm::vec3(12.92f);
		
		return glm::mix(higher, lower, cutoff);
	}

	inline glm::vec3 srgb_to_linear(const glm::vec3& srgb)
	{
		glm::vec3 clamped = glm::clamp(srgb, 0.0f, 1.0f);

		glm::bvec3 cutoff = lessThan(clamped, glm::vec3(0.04045f));
		glm::vec3 higher = glm::pow((clamped + glm::vec3(0.055f)) / glm::vec3(1.055f), glm::vec3(2.4f));
		glm::vec3 lower = clamped / glm::vec3(12.92f);

		return glm::mix(higher, lower, cutoff);
	}

	/*
		POSTFX
	*/

	inline glm::vec3 tonemap_reinhard(const glm::vec3& color)
	{
		return color / (1.0f + color);
	}

	inline glm::vec3 tonemap_reinhard_white(const glm::vec3& color, f32 max_white)
	{
		f32 max_white_sq = max_white * max_white;
		glm::vec3 numerator = color * (1.0f + (color / glm::vec3(max_white_sq)));
		return numerator / (1.0f + color);
	}

	inline glm::vec3 apply_contrast_brightness(const glm::vec3& color, f32 contrast, f32 brightness)
	{
		return glm::clamp(contrast * (color - 0.5f) + 0.5f + brightness, 0.0f, 1.0f);
	}

	inline glm::vec3 apply_saturation(const glm::vec3& color, f32 saturation)
	{
		f32 grayscale = glm::dot(color, glm::vec3(0.299f, 0.587f, 0.114f));
		return glm::clamp(glm::mix(glm::vec3(grayscale), color, saturation), 0.0f, 1.0f);
	}

}

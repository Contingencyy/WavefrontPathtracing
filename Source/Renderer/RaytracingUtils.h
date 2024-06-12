#pragma once
#include "RaytracingTypes.h"
#include "Core/Camera/Camera.h"
#include "Core/Random.h"

namespace RTUtil
{

	/*
		INTERSECTION FUNCTIONS
	*/

	inline bool Intersect(const Triangle& tri, Ray& ray)
	{
		float t = 0.0f;
		glm::vec3 edge1 = tri.p1 - tri.p0;
		glm::vec3 edge2 = tri.p2 - tri.p0;

		glm::vec3 H = glm::cross(ray.dir, edge2);
		float a = glm::dot(edge1, H);

		if (std::fabs(a) < 0.001f)
			return false;

		float f = 1.0f / a;
		glm::vec3 S = ray.origin - tri.p0;
		float u = f * glm::dot(S, H);

		if (u < 0.0f || u > 1.0f)
			return false;

		glm::vec3 Q = glm::cross(S, edge1);
		float v = f * glm::dot(ray.dir, Q);

		if (v < 0.0f || u + v > 1.0f)
			return false;

		t = f * glm::dot(edge2, Q);

		if (t > 0.0f && t < ray.t)
		{
			ray.t = t;
			return true;
		}

		return false;
	}

	inline bool Intersect(const Sphere& sphere, Ray& ray)
	{
		float t0 = 0.0f, t1 = 0.0f;
		glm::vec3 L = sphere.center - ray.origin;
		float tca = glm::dot(L, ray.dir);

		if (tca < 0.0f)
			return false;

		float d2 = glm::dot(L, L) - tca * tca;

		if (d2 > sphere.radiusSquared)
			return false;

		float thc = std::sqrtf(sphere.radiusSquared - d2);
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

	inline bool Intersect(const Plane& plane, Ray& ray)
	{
		float denom = glm::dot(ray.dir, plane.normal);

		if (std::fabs(denom) > 1e-6)
		{
			glm::vec3 p0 = plane.point - ray.origin;
			float t = glm::dot(p0, plane.normal) / denom;

			if (t > 0.0f && t < ray.t)
			{
				ray.t = t;
				return true;
			}
		}

		return false;
	}

	inline bool Intersect(const AABB& aabb, Ray& ray)
	{
		float tx1 = (aabb.pmin.x - ray.origin.x) * ray.invDir.x, tx2 = (aabb.pmax.x - ray.origin.x) * ray.invDir.x;
		float tmin = std::min(tx1, tx2), tmax = std::max(tx1, tx2);
		float ty1 = (aabb.pmin.y - ray.origin.y) * ray.invDir.y, ty2 = (aabb.pmax.y - ray.origin.y) * ray.invDir.y;
		tmin = std::max(tmin, std::min(ty1, ty2)), tmax = std::min(tmax, std::max(ty1, ty2));
		float tz1 = (aabb.pmin.z - ray.origin.z) * ray.invDir.z, tz2 = (aabb.pmax.z - ray.origin.z) * ray.invDir.z;
		tmin = std::max(tmin, std::min(tz1, tz2)), tmax = std::min(tmax, std::max(tz1, tz2));

		if (tmax >= tmin && tmin < ray.t && tmax > 0.0f)
		{
			ray.t = tmin;
			return true;
		}

		return false;
	}

	inline float IntersectSSE(const __m128 aabbMin, const __m128 aabbMax, Ray& ray)
	{
		__m128 mask4 = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_set_ps(1, 0, 0, 0));

		__m128 t1 = _mm_mul_ps(_mm_sub_ps(_mm_and_ps(aabbMin, mask4), ray.origin4), ray.invDir4);
		__m128 t2 = _mm_mul_ps(_mm_sub_ps(_mm_and_ps(aabbMax, mask4), ray.origin4), ray.invDir4);
		__m128 vmax4 = _mm_max_ps(t1, t2), vmin4 = _mm_min_ps(t1, t2);

		float tmax = std::min(vmax4.m128_f32[0], std::min(vmax4.m128_f32[1], vmax4.m128_f32[2]));
		float tmin = std::max(vmin4.m128_f32[0], std::max(vmin4.m128_f32[1], vmin4.m128_f32[2]));

		if (tmax >= tmin && tmin < ray.t && tmax > 0.0f)
		{
			return tmin;
		}

		return FLT_MAX;
	}

	/*
		HIT SURFACE
	*/

	inline glm::vec3 GetHitNormal(const Triangle& tri, const glm::vec3& hitPos)
	{
		return glm::normalize(glm::cross(tri.p1 - tri.p0, tri.p2 - tri.p0));
	}

	inline glm::vec3 GetHitNormal(const Sphere& sphere, const glm::vec3& hitPos)
	{
		return glm::normalize(hitPos - sphere.center);
	}

	inline glm::vec3 GetHitNormal(const Plane& plane, const glm::vec3& hitPos)
	{
		return plane.normal;
	}

	inline glm::vec3 GetHitNormal(const AABB& aabb, const glm::vec3& hitPos)
	{
		const glm::vec3 center = (aabb.pmax + aabb.pmin) * 0.5f;
		const glm::vec3 halfSize = (aabb.pmax - aabb.pmin) * 0.5f;
		const glm::vec3 centerToHit = hitPos - center;

		return glm::normalize(glm::sign(centerToHit) * glm::step(-RAY_NUDGE_MODIFIER, glm::abs(centerToHit) - halfSize));
	}

	/*
		CAMERA
	*/

	inline Ray ConstructCameraRay(const Camera& camera, uint32_t pixelX, uint32_t pixelY, float tanFOV,
		float aspectRatio, float invScreenWidth, float invScreenHeight)
	{
		// Calculate UV of the pixel coordinate on the screen plane, 0..1 range,
		// and apply a 0.5 offset so that we get the center of the pixel and not its top-left corner
		// Note that unlike in rasterization, the NDC coordinates range from 0..1 instead of -1..1
		float u = (pixelX + 0.5f) * invScreenWidth;
		float v = (pixelY + 0.5f) * invScreenHeight;

		// Remap pixel positions from 0..1 to -1..1
		glm::vec2 pixelViewPos = glm::vec2(2.0f * u - 1.0f, 1.0f - 2.0f * v);
		// Apply aspect ratio by scaling the pixel X coordinate in NDC space and FOV
		pixelViewPos.x *= aspectRatio;
		pixelViewPos.x *= tanFOV;
		pixelViewPos.y *= tanFOV;

		// Transform pixel view-space position and direction to world-space
		glm::vec3 cameraToPixelDirWorld = glm::vec3(camera.transformMatrix * glm::vec4(pixelViewPos.x, pixelViewPos.y, 1.0f, 0.0f));
		glm::vec3 cameraOriginWorld = glm::vec3(camera.transformMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		cameraToPixelDirWorld = glm::normalize(cameraToPixelDirWorld);

		return Ray(cameraOriginWorld, cameraToPixelDirWorld);
	}

	/*
		SAMPLING
	*/

	inline void CreateOrthonormalBasis(const glm::vec3& normal, glm::vec3& tangent, glm::vec3& bitangent)
	{
		if (glm::abs(normal.x) > glm::abs(normal.z))
			tangent = glm::normalize(glm::vec3(-normal.y, normal.x, 0.0f));
		else
			tangent = glm::normalize(glm::vec3(0.0f, -normal.z, normal.y));

		bitangent = glm::cross(normal, tangent);
	}

	inline glm::vec3 DirectionToNormalSpace(const glm::vec3& normal, const glm::vec3& sampleDir)
	{
		glm::vec3 tangent, bitangent;
		CreateOrthonormalBasis(normal, tangent, bitangent);

		return glm::vec3(sampleDir.x * tangent + sampleDir.y * normal + sampleDir.z * bitangent);
	}

	inline glm::vec3 UniformHemisphereSample(const glm::vec3& normal)
	{
		float r1 = Random::Float();
		float r2 = Random::Float();

		float sinTheta = glm::sqrt(1.0f - r1 * r1);
		float phi = 2.0f * PI * r2;

		return DirectionToNormalSpace(normal, glm::vec3(sinTheta * glm::cos(phi), r1, sinTheta * glm::sin(phi)));
	}

	inline glm::vec3 CosineWeightedHemisphereSample(const glm::vec3& normal)
	{
		float r1 = Random::Float();
		float r2 = Random::Float();

		float cosTheta = glm::sqrt(r1);
		float sinTheta = glm::sqrt(1.0f - cosTheta * cosTheta);
		float phi = 2.0f * PI * r2;

		return DirectionToNormalSpace(normal, glm::vec3(sinTheta * glm::cos(phi), cosTheta, sinTheta * glm::sin(phi)));
	}

	inline glm::vec3 Reflect(const glm::vec3& inDir, const glm::vec3& normal)
	{
		return inDir - 2.0f * normal * glm::dot(inDir, normal);
	}

	inline glm::vec3 Refract(const glm::vec3& dir, const glm::vec3& normal, float eta, float cosi, float k)
	{
		return glm::normalize(dir * eta + ((eta * cosi - glm::sqrt(k)) * normal));
	}

	inline float Fresnel(float in, float out, float iorOutside, float iorInside)
	{
		float sPolarized = (iorOutside * in - iorInside * out) /
			(iorOutside * in + iorInside * out);
		float pPolarized = (iorOutside * out - iorInside * in) /
			(iorOutside * out + iorInside * in);
		return 0.5f * ((sPolarized * sPolarized) + (pPolarized * pPolarized));
	}

	inline float SurvivalProbabilityRR(const glm::vec3& albedo)
	{
		return glm::clamp(glm::max(glm::max(albedo.x, albedo.y), albedo.z), 0.0f, 1.0f);
	}

	/*
		COLOR
	*/

	inline uint32_t Vec4ToUint32(const glm::vec4& rgba)
	{
		uint8_t r = static_cast<uint8_t>(255.0f * std::min(1.0f, rgba.r));
		uint8_t g = static_cast<uint8_t>(255.0f * std::min(1.0f, rgba.g));
		uint8_t b = static_cast<uint8_t>(255.0f * std::min(1.0f, rgba.b));
		uint8_t a = static_cast<uint8_t>(255.0f * std::min(1.0f, rgba.a));
		return (a << 24) | (b << 16) | (g << 8) | r;
	}

	inline glm::vec3 LinearToSRGB(const glm::vec3& color)
	{
		glm::vec3 clamped = glm::clamp(color, 0.0f, 1.0f);
		return glm::mix(
			glm::pow(clamped, glm::vec3((1.0f / 2.4f) * 1.055f - 0.055f)),
			clamped * 12.92f,
			glm::lessThan(clamped, glm::vec3(0.0031308f))
		);
	}

	inline glm::vec3 SRGBToLinear(const glm::vec3& color)
	{
		glm::vec3 clamped = glm::clamp(color, 0.0f, 1.0f);
		return glm::mix(
			glm::pow((clamped + 0.055f) / 1.055f, glm::vec3(2.4f)),
			clamped / 12.92f,
			glm::lessThan(clamped, glm::vec3(0.04045f))
		);
	}

	/*
		POSTFX
	*/

	inline glm::vec3 TonemapReinhard(const glm::vec3& color)
	{
		return color / (1.0f + color);
	}

	inline glm::vec3 TonemapReinhardWhite(const glm::vec3& color, float maxWhite)
	{
		float maxWhiteSquared = maxWhite * maxWhite;
		glm::vec3 numerator = color * (1.0f + (color / glm::vec3(maxWhiteSquared)));
		return numerator / (1.0f + color);
	}

	inline glm::vec3 AdjustContrastBrightness(const glm::vec3& color, float contrast, float brightness)
	{
		return glm::clamp(contrast * (color - 0.5f) + 0.5f + brightness, 0.0f, 1.0f);
	}

	inline glm::vec3 Saturate(const glm::vec3& color, float saturation)
	{
		float grayscale = glm::dot(color, glm::vec3(0.299f, 0.587f, 0.114f));
		return glm::clamp(glm::mix(glm::vec3(grayscale), color, saturation), 0.0f, 1.0f);
	}

}

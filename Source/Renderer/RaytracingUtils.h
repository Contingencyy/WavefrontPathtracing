#pragma once
#include "RaytracingTypes.h"
#include "Camera.h"

namespace RTUtil
{

	static constexpr float RAY_MAX_T = FLT_MAX;

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

	/*
		CAMERA
	*/

	inline Ray ConstructCameraRay(const Camera& camera, uint32_t pixelX, uint32_t pixelY,
		float aspectRatio, float invScreenWidth, float invScreenHeight)
	{
		// Calculate UV of the pixel coordinate on the screen plane, 0..1 range,
		// and apply a 0.5 offset so that we get the center of the pixel and not its top-left corner
		// Note that unlike in rasterization, the NDC coordinates range from 0..1 instead of -1..1
		float u = (pixelX + 0.5f) * invScreenWidth;
		float v = (pixelY + 0.5f) * invScreenHeight;

		// Remap pixel positions from 0..1 to -1..1
		glm::vec4 pixelViewPos = glm::vec4(2.0f * u - 1.0f, 1.0f - 2.0f * v, 1.0f, 1.0f);
		// Apply aspect ratio by scaling the pixel X coordinate in NDC space
		pixelViewPos.x *= aspectRatio;
		// Apply FOV to the pixel NDC coordinates
		pixelViewPos.x *= glm::tan(glm::radians(camera.vfov) / 2.0f);
		pixelViewPos.y *= glm::tan(glm::radians(camera.vfov) / 2.0f);

		// Transform pixel view-space position and direction to world-space
		glm::vec3 cameraToPixelDirWorld = glm::vec3(camera.invViewMatrix * glm::vec4(pixelViewPos.x, pixelViewPos.y, 1.0f, 0.0f));
		glm::vec3 cameraOriginWorld = glm::vec3(camera.invViewMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		cameraToPixelDirWorld = glm::normalize(cameraToPixelDirWorld);

		return Ray{
			.origin = cameraOriginWorld,
			.dir = cameraToPixelDirWorld,
			.invDir = 1.0f / cameraToPixelDirWorld,
			.t = RAY_MAX_T
		};
	}

	inline uint32_t Vec4ToUint32(const glm::vec4& rgba)
	{
		uint8_t r = static_cast<uint8_t>(255.0f * std::min(1.0f, rgba.r));
		uint8_t g = static_cast<uint8_t>(255.0f * std::min(1.0f, rgba.g));
		uint8_t b = static_cast<uint8_t>(255.0f * std::min(1.0f, rgba.b));
		uint8_t a = static_cast<uint8_t>(255.0f * std::min(1.0f, rgba.a));
		return (a << 24) + (b << 16) + (g << 8) + r;
	}

}

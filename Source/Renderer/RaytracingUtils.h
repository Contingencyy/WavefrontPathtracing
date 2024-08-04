#pragma once
#include "RaytracingTypes.h"
#include "Core/Camera/Camera.h"
#include "Core/Random.h"

namespace RTUtil
{

	/*
		INTERSECTION FUNCTIONS
	*/

	// Möller-Trumbore
	inline b8 Intersect(const Triangle& Tri, Ray& Ray, f32& OutT, glm::vec3& OutBary)
	{
		// This algorithm is very sensitive to Epsilon values, so we need a very small one, otherwise transforming/scaling the Ray breaks this
		const f32 Epsilon = 0.00000000001f;

		// First check whether we hit the Plane the Tri is on
		glm::vec3 Edge1 = Tri.p1 - Tri.p0;
		glm::vec3 Edge2 = Tri.p2 - Tri.p0;

		glm::vec3 h = glm::cross(Ray.Dir, Edge2);
		f32 det = glm::dot(Edge1, h);

		if (glm::abs(det) < Epsilon)
			return false;

		// Then check whether the Ray-Plane intersection lies outside of the Tri using barycentric coordinates
		f32 f = 1.0f / det;
		glm::vec3 s = Ray.Origin - Tri.p0;
		f32 v = f * glm::dot(s, h);

		if (v < 0.0f || v > 1.0f)
			return false;

		glm::vec3 q = glm::cross(s, Edge1);
		f32 w = f * glm::dot(Ray.Dir, q);

		if (w < 0.0f || v + w > 1.0f)
			return false;

		// Ray-Plane intersection is inside the Tri, so we can compute "t" now
		f32 t = f * glm::dot(Edge2, q);

		if (t < Epsilon || t >= Ray.t)
			return false;

		Ray.t = t;
		OutT = t;
		OutBary = glm::vec3(1.0f - v - w, v, w);

		return true;
	}

	inline b8 Intersect(const Sphere& Sphere, Ray& Ray)
	{
		f32 t0 = 0.0f, t1 = 0.0f;
		glm::vec3 L = Sphere.Center - Ray.Origin;
		f32 tca = glm::dot(L, Ray.Dir);

		if (tca < 0.0f)
			return false;

		f32 d2 = glm::dot(L, L) - tca * tca;

		if (d2 > Sphere.RadiusSquared)
			return false;

		f32 thc = std::sqrtf(Sphere.RadiusSquared - d2);
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

		if (t0 < Ray.t)
		{
			Ray.t = t0;
			return true;
		}

		return false;
	}

	inline b8 Intersect(const Plane& Plane, Ray& Ray)
	{
		f32 det = glm::dot(Ray.Dir, Plane.Normal);

		if (glm::abs(det) > 1e-6)
		{
			glm::vec3 p0 = Plane.Point - Ray.Origin;
			f32 t = glm::dot(p0, Plane.Normal) / det;

			if (t > 0.0f && t < Ray.t)
			{
				Ray.t = t;
				return true;
			}
		}

		return false;
	}

	inline b8 Intersect(const AABB& Aabb, Ray& Ray)
	{
		f32 tx1 = (Aabb.PMin.x - Ray.Origin.x) * Ray.InvDir.x, tx2 = (Aabb.PMax.x - Ray.Origin.x) * Ray.InvDir.x;
		f32 tmin = std::min(tx1, tx2), tmax = std::max(tx1, tx2);
		f32 ty1 = (Aabb.PMin.y - Ray.Origin.y) * Ray.InvDir.y, ty2 = (Aabb.PMax.y - Ray.Origin.y) * Ray.InvDir.y;
		tmin = std::max(tmin, std::min(ty1, ty2)), tmax = std::min(tmax, std::max(ty1, ty2));
		f32 tz1 = (Aabb.PMin.z - Ray.Origin.z) * Ray.InvDir.z, tz2 = (Aabb.PMax.z - Ray.Origin.z) * Ray.InvDir.z;
		tmin = std::max(tmin, std::min(tz1, tz2)), tmax = std::min(tmax, std::max(tz1, tz2));

		if (tmax >= tmin && tmin < Ray.t && tmax > 0.0f)
		{
			Ray.t = tmin;
			return true;
		}

		return false;
	}

	inline f32 IntersectSSE(const __m128 AabbMin, const __m128 AabbMax, const Ray& Ray)
	{
		__m128 mask4 = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_set_ps(1, 0, 0, 0));

		__m128 t1 = _mm_mul_ps(_mm_sub_ps(_mm_and_ps(AabbMin, mask4), Ray.Origin4), Ray.InvDir4);
		__m128 t2 = _mm_mul_ps(_mm_sub_ps(_mm_and_ps(AabbMax, mask4), Ray.Origin4), Ray.InvDir4);
		__m128 vmax4 = _mm_max_ps(t1, t2), vmin4 = _mm_min_ps(t1, t2);

		f32 tmax = std::min(vmax4.m128_f32[0], std::min(vmax4.m128_f32[1], vmax4.m128_f32[2]));
		f32 tmin = std::max(vmin4.m128_f32[0], std::max(vmin4.m128_f32[1], vmin4.m128_f32[2]));

		if (tmax >= tmin && tmin < Ray.t && tmax > 0.0f)
		{
			return tmin;
		}

		return FLT_MAX;
	}

	/*
		HIT SURFACE
	*/

	inline glm::vec3 GetHitNormal(const Triangle& Tri, const glm::vec3& Bary)
	{
		return glm::normalize(glm::vec3(Bary.x * Tri.n0 + Bary.y * Tri.n1 + Bary.z * Tri.n2));
	}

	inline glm::vec3 GetHitNormal(const Sphere& Sphere, const glm::vec3& HitPos)
	{
		return glm::normalize(HitPos - Sphere.Center);
	}

	inline glm::vec3 GetHitNormal(const Plane& plane)
	{
		return plane.Normal;
	}

	inline glm::vec3 GetHitNormal(const AABB& Aabb, const glm::vec3& HitPos)
	{
		const glm::vec3 center = (Aabb.PMax + Aabb.PMin) * 0.5f;
		const glm::vec3 halfSize = (Aabb.PMax - Aabb.PMin) * 0.5f;
		const glm::vec3 centerToHit = HitPos - center;

		return glm::normalize(glm::sign(centerToHit) * glm::step(-RAY_NUDGE_MODIFIER, glm::abs(centerToHit) - halfSize));
	}

	/*
		PRIMITIVES
	*/

	inline glm::vec3 GetTriangleCentroid(const Triangle& Tri)
	{
		return (Tri.p0 + Tri.p1 + Tri.p2) * 0.3333f;
	}

	inline void GetTriangleMinMax(const Triangle& Tri, glm::vec3& OutMin, glm::vec3& OutMax)
	{
		OutMin = glm::min(Tri.p0, Tri.p1);
		OutMax = glm::max(Tri.p0, Tri.p1);

		OutMin = glm::min(OutMin, Tri.p2);
		OutMax = glm::max(OutMax, Tri.p2);
	}

	inline f32 GetAABBVolume(const glm::vec3& AabbMin, const glm::vec3& AabbMax)
	{
		const glm::vec3 Extent = AabbMax - AabbMin;
		return Extent.x * Extent.y + Extent.y * Extent.z + Extent.z * Extent.x;
	}

	inline void GrowAABB(glm::vec3& AabbMin, glm::vec3& AabbMax, const glm::vec3& Pos)
	{
		AabbMin = glm::min(AabbMin, Pos);
		AabbMax = glm::max(AabbMax, Pos);
	}

	inline void GrowAABB(glm::vec3& AabbMin, glm::vec3& AabbMax, const glm::vec3& OtherMin, const glm::vec3& OtherMax)
	{
		AabbMin = glm::min(AabbMin, OtherMin);
		AabbMax = glm::max(AabbMax, OtherMax);
	}

	/*
		CAMERA
	*/

	inline Ray ConstructCameraRay(const Camera& Camera, u32 PixelX, u32 PixelY, f32 TanFOV,
		f32 AspectRatio, f32 InvScreenWidth, f32 InvScreenHeight)
	{
		// Calculate UV of the pixel coordinate on the screen Plane, 0..1 range,
		// and apply a 0.5 offset so that we get the Center of the pixel and not its top-left corner
		// Note that unlike In rasterization, the NDC coordinates range from 0..1 instead of -1..1
		f32 u = (PixelX + 0.5f) * InvScreenWidth;
		f32 v = (PixelY + 0.5f) * InvScreenHeight;

		// Remap pixel positions from 0..1 to -1..1
		glm::vec2 pixelViewPos = glm::vec2(2.0f * u - 1.0f, 1.0f - 2.0f * v);
		// Apply aspect ratio by scaling the pixel X coordinate In NDC space and FOV
		pixelViewPos.x *= AspectRatio;
		pixelViewPos.x *= TanFOV;
		pixelViewPos.y *= TanFOV;

		// Transform pixel view-space Position and direction to world-space
		glm::vec3 cameraToPixelDirWorld = glm::vec3(Camera.TransformMatrix * glm::vec4(pixelViewPos.x, pixelViewPos.y, 1.0f, 0.0f));
		glm::vec3 cameraOriginWorld = glm::vec3(Camera.TransformMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		cameraToPixelDirWorld = glm::normalize(cameraToPixelDirWorld);

		return MakeRay(cameraOriginWorld, cameraToPixelDirWorld);
	}

	/*
		SAMPLING
	*/

	inline void CreateOrthonormalBasis(const glm::vec3& Normal, glm::vec3& Tangent, glm::vec3& Bitangent)
	{
		if (glm::abs(Normal.x) > glm::abs(Normal.z))
			Tangent = glm::normalize(glm::vec3(-Normal.y, Normal.x, 0.0f));
		else
			Tangent = glm::normalize(glm::vec3(0.0f, -Normal.z, Normal.y));

		Bitangent = glm::cross(Normal, Tangent);
	}

	inline glm::vec3 DirectionToNormalSpace(const glm::vec3& Normal, const glm::vec3& SampleDir)
	{
		glm::vec3 tangent, bitangent;
		CreateOrthonormalBasis(Normal, tangent, bitangent);

		return glm::vec3(SampleDir.x * tangent + SampleDir.y * Normal + SampleDir.z * bitangent);
	}

	inline glm::vec3 UniformHemisphereSample(const glm::vec3& Normal)
	{
		f32 R1 = Random::Float();
		f32 R2 = Random::Float();

		f32 SinTheta = glm::sqrt(1.0f - R1 * R1);
		f32 Phi = 2.0f * PI * R2;

		return DirectionToNormalSpace(Normal, glm::vec3(SinTheta * glm::cos(Phi), R1, SinTheta * glm::sin(Phi)));
	}

	inline glm::vec3 CosineWeightedHemisphereSample(const glm::vec3& Normal)
	{
		f32 R1 = Random::Float();
		f32 R2 = Random::Float();

		f32 CosTheta = glm::sqrt(R1);
		f32 SinTheta = glm::sqrt(1.0f - CosTheta * CosTheta);
		f32 Phi = 2.0f * PI * R2;

		return DirectionToNormalSpace(Normal, glm::vec3(SinTheta * glm::cos(Phi), CosTheta, SinTheta * glm::sin(Phi)));
	}

	inline glm::vec3 Reflect(const glm::vec3& InDir, const glm::vec3& Normal)
	{
		return InDir - 2.0f * Normal * glm::dot(InDir, Normal);
	}

	inline glm::vec3 Refract(const glm::vec3& Dir, const glm::vec3& Normal, f32 eta, f32 cosi, f32 k)
	{
		return glm::normalize(Dir * eta + ((eta * cosi - glm::sqrt(k)) * Normal));
	}

	inline f32 Fresnel(f32 In, f32 Out, f32 IorOutside, f32 IorInside)
	{
		f32 sPolarized = (IorOutside * In - IorInside * Out) /
			(IorOutside * In + IorInside * Out);
		f32 pPolarized = (IorOutside * Out - IorInside * In) /
			(IorOutside * Out + IorInside * In);
		return 0.5f * ((sPolarized * sPolarized) + (pPolarized * pPolarized));
	}

	inline f32 SurvivalProbabilityRR(const glm::vec3& Albedo)
	{
		return glm::clamp(glm::max(glm::max(Albedo.x, Albedo.y), Albedo.z), 0.0f, 1.0f);
	}

	inline glm::vec2 DirectionToEquirectangularUV(const glm::vec3& Dir)
	{
		glm::vec2 uv = glm::vec2(glm::atan(Dir.z, Dir.x), glm::asin(Dir.y));
		uv *= INV_ATAN;
		uv += 0.5f;

		return uv;
	}

	/*
		COLOR
	*/

	inline u32 Vec4ToUint32(const glm::vec4& Rgba)
	{
		u8 r = static_cast<u8>(255.0f * std::min(1.0f, Rgba.r));
		u8 g = static_cast<u8>(255.0f * std::min(1.0f, Rgba.g));
		u8 b = static_cast<u8>(255.0f * std::min(1.0f, Rgba.b));
		u8 a = static_cast<u8>(255.0f * std::min(1.0f, Rgba.a));
		return (a << 24) | (b << 16) | (g << 8) | r;
	}

	inline glm::vec3 LinearToSRGB(const glm::vec3& Linear)
	{
		glm::vec3 Clamped = glm::clamp(Linear, 0.0f, 1.0f);

		glm::bvec3 Cutoff = glm::lessThan(Clamped, glm::vec3(0.0031308f));
		glm::vec3 Higher = glm::vec3(1.055f) * glm::pow(Clamped, glm::vec3(1.0f / 2.4f)) - glm::vec3(0.055f);
		glm::vec3 Lower = Clamped * glm::vec3(12.92f);
		
		return glm::mix(Higher, Lower, Cutoff);
	}

	inline glm::vec3 SRGBToLinear(const glm::vec3& srgb)
	{
		glm::vec3 Clamped = glm::clamp(srgb, 0.0f, 1.0f);

		glm::bvec3 Cutoff = lessThan(Clamped, glm::vec3(0.04045f));
		glm::vec3 Higher = glm::pow((Clamped + glm::vec3(0.055f)) / glm::vec3(1.055f), glm::vec3(2.4f));
		glm::vec3 Lower = Clamped / glm::vec3(12.92f);

		return glm::mix(Higher, Lower, Cutoff);
	}

	/*
		POSTFX
	*/

	inline glm::vec3 TonemapReinhard(const glm::vec3& Color)
	{
		return Color / (1.0f + Color);
	}

	inline glm::vec3 TonemapReinhardWhite(const glm::vec3& Color, f32 MaxWhite)
	{
		f32 maxWhiteSquared = MaxWhite * MaxWhite;
		glm::vec3 numerator = Color * (1.0f + (Color / glm::vec3(maxWhiteSquared)));
		return numerator / (1.0f + Color);
	}

	inline glm::vec3 AdjustContrastBrightness(const glm::vec3& Color, f32 Contrast, f32 Brightness)
	{
		return glm::clamp(Contrast * (Color - 0.5f) + 0.5f + Brightness, 0.0f, 1.0f);
	}

	inline glm::vec3 Saturate(const glm::vec3& Color, f32 Saturation)
	{
		f32 grayscale = glm::dot(Color, glm::vec3(0.299f, 0.587f, 0.114f));
		return glm::clamp(glm::mix(glm::vec3(grayscale), Color, Saturation), 0.0f, 1.0f);
	}

}

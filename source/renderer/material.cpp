#include "pch.h"
#include "material.h"

namespace material
{

	material_t make_diffuse(const glm::vec3& albedo)
	{
		material_t material = {};
		material.albedo = albedo;

		return material;
	}

	material_t make_specular(const glm::vec3& albedo, f32 spec)
	{
		material_t material = {};
		material.albedo = albedo;
		material.specular = spec;

		return material;
	}

	material_t make_refractive(const glm::vec3& albedo, f32 spec, f32 refract, f32 ior, const glm::vec3& absorption)
	{
		material_t material = {};
		material.albedo = albedo;
		material.specular = spec;
		material.refractivity = refract;
		material.ior = ior;
		material.absorption = absorption;

		return material;
	}

	material_t make_emissive(const glm::vec3& emissive_color, f32 emissive_intensity)
	{
		material_t material = {};
		material.emissive = true;
		material.emissive_color = emissive_color;
		material.emissive_intensity = emissive_intensity;

		return material;
	}

}

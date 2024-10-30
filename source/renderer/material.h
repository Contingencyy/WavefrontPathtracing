#pragma once
#include "core/common.h"

struct material_t
{
	glm::vec3 albedo = {};
	f32 specular = 0.0f;

	f32 refractivity = 0.0f;
	f32 ior = 1.0f;
	glm::vec3 absorption = {};

	b8 emissive = false;
	glm::vec3 emissive_color = {};
	f32 emissive_intensity = 0.0f;
};

namespace material
{

	material_t make_diffuse(const glm::vec3& albedo);
	material_t make_specular(const glm::vec3& albedo, f32 spec);
	material_t make_refractive(const glm::vec3& albedo, f32 spec, f32 refract, f32 ior, const glm::vec3& absorption);
	material_t make_emissive(const glm::vec3& emissive_color, f32 emissive_intensity);

}

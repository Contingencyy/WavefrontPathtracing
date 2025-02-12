#pragma once
#include "core/common.h"
#include "renderer/shaders/shared.hlsl.h"

namespace material
{

	material_t make_diffuse(const glm::vec3& albedo);
	material_t make_specular(const glm::vec3& albedo, float spec);
	material_t make_refractive(const glm::vec3& albedo, float spec, float refract, float ior, const glm::vec3& absorption);
	material_t make_emissive(const glm::vec3& emissive_color, float emissive_intensity);

}

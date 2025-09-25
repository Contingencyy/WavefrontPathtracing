#pragma once

#include "core/math/math.h"

struct camera_t
{
	glm::mat4 inv_view_matrix;
	glm::mat4 view_matrix;

	float vfov_deg;
	float yaw;
	float pitch;
};

namespace camera
{

	void create(camera_t& camera, const glm::vec3& view_origin, const glm::vec3& target_pos, float vfov_deg);
	void destroy(camera_t& camera);

}

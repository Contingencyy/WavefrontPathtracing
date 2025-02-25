#include "pch.h"
#include "camera.h"
#include "core/common.h"

namespace camera
{

	void create(camera_t& camera, const glm::vec3& view_origin, const glm::vec3& target_pos, float vfov_deg)
	{
		camera.vfov_deg = vfov_deg;
		camera.yaw = 0.0f;
		camera.pitch = 0.0f;

		camera.view_matrix = glm::lookAtLH(view_origin, target_pos, DEFAULT_UP_VECTOR);
		camera.inv_view_matrix = glm::inverse(camera.view_matrix);
	}

	void destroy(camera_t& camera)
	{
		camera.vfov_deg = 0.0f;
		camera.yaw = 0.0f;
		camera.pitch = 0.0f;
		camera.view_matrix = glm::identity<glm::mat4>();
		camera.inv_view_matrix = glm::identity<glm::mat4>();
	}

}

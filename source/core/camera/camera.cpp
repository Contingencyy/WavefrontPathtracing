#include "pch.h"
#include "camera.h"
#include "core/common.h"

camera_t::camera_t()
	: camera_t(glm::vec3(), glm::vec3(0.0f, 0.0f, 1.0f), 60.0f)
{
}

camera_t::camera_t(const glm::vec3& eye_pos, const glm::vec3& lookat_pos, f32 vfov_deg)
	: vfov_deg(vfov_deg)
{
	view_mat = glm::lookAtLH(eye_pos, lookat_pos, DEFAULT_UP_VECTOR);
	transform_mat = glm::inverse(view_mat);
}

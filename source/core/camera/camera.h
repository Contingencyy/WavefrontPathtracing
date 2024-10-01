#pragma once

struct camera_t
{
	camera_t();
	// Vertical FOV in degrees
	camera_t(const glm::vec3& eye_pos, const glm::vec3& lookat_pos, f32 vfov_deg);

	glm::mat4 transform_mat = {};
	glm::mat4 view_mat = {};
	
	f32 vfov_deg = 60.0f;
	f32 yaw = 0.0f;
	f32 pitch = 0.0f;

	struct screen_planes_t
	{
		glm::vec3 center = {};
		glm::vec3 top_left = {};
		glm::vec3 top_right = {};
		glm::vec3 bottom_left = {};
	} screen_planes;

};

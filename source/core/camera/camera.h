#pragma once

struct camera_t
{
	camera_t();
	// Vertical FOV in degrees
	camera_t(const glm::vec3& eye_pos, const glm::vec3& lookat_pos, float vfov_deg);

	glm::mat4 transform_mat = {};
	glm::mat4 view_mat = {};
	
	float vfov_deg = 60.0f;
	float yaw = 0.0f;
	float pitch = 0.0f;

	struct screen_planes_t
	{
		glm::vec3 center = {};
		glm::vec3 top_left = {};
		glm::vec3 top_right = {};
		glm::vec3 bottom_left = {};
	} screen_planes;

};

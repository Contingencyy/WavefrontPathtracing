#pragma once

struct Camera
{
	Camera();
	// Vertical FOV in degrees
	Camera(const glm::vec3& eyePos, const glm::vec3& lookAt, float vfov);

	glm::mat4 transformMatrix = {};
	glm::mat4 viewMatrix = {};
	
	float vfov = 60.0f;
	float yaw = 0.0f;
	float pitch = 0.0f;

	glm::vec3 screenPlaneCenter = {};
	glm::vec3 screenPlaneTopLeft = {};
	glm::vec3 screenPlaneTopRight = {};
	glm::vec3 screenPlaneBottomLeft = {};
};

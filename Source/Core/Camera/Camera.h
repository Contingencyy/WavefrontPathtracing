#pragma once

struct Camera
{
	Camera();
	// Vertical FOV in degrees
	Camera(const glm::vec3& EyePos, const glm::vec3& LookAtPos, f32 VFovDeg);

	glm::mat4 TransformMatrix = {};
	glm::mat4 ViewMatrix = {};
	
	f32 VFovDeg = 60.0f;
	f32 Yaw = 0.0f;
	f32 Pitch = 0.0f;

	struct ScreenPlanes
	{
		glm::vec3 Center = {};
		glm::vec3 TopLeft = {};
		glm::vec3 TopRight = {};
		glm::vec3 BottomLeft = {};
	} ScreenPlanes;

};

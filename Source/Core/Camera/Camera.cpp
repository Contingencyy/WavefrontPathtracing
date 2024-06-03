#include "Pch.h"
#include "Camera.h"
#include "Core/Common.h"

Camera::Camera()
	: Camera(glm::vec3(), glm::vec3(0.0f, 0.0f, 1.0f), vfov)
{
}

Camera::Camera(const glm::vec3& eyePos, const glm::vec3& lookAt, float vfov)
	: vfov(vfov)
{
	viewMatrix = glm::lookAtLH(eyePos, lookAt, DEFAULT_UP_VECTOR);
	transformMatrix = glm::inverse(viewMatrix);
}

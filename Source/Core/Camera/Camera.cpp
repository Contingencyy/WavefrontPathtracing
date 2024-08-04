#include "Pch.h"
#include "Camera.h"
#include "Core/Common.h"

Camera::Camera()
	: Camera(glm::vec3(), glm::vec3(0.0f, 0.0f, 1.0f), 60.0f)
{
}

Camera::Camera(const glm::vec3& EyePos, const glm::vec3& LookAtPos, f32 VFovDeg)
	: VFovDeg(VFovDeg)
{
	ViewMatrix = glm::lookAtLH(EyePos, LookAtPos, DEFAULT_UP_VECTOR);
	TransformMatrix = glm::inverse(ViewMatrix);
}

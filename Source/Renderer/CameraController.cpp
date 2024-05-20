#include "Pch.h"
#include "CameraController.h"
#include "Core/Input.h"

CameraController::CameraController(const Camera& camera)
	: m_Camera(camera)
{
}

void CameraController::Update(float dt)
{
	if (Input::IsMouseCaptured())
	{
		// Mouse wheel to increase/decrease camera move speed
		float scrollY = Input::GetMouseScrollRelY();
		m_CameraMoveSpeed += 0.01f * scrollY * std::max(std::sqrtf(m_CameraMoveSpeed), 0.005f);
		m_CameraMoveSpeed = std::max(m_CameraMoveSpeed, 0.0f);

		// Rotation
		float mouseX = Input::GetMouseRelX();
		float mouseY = Input::GetMouseRelY();

		float yawSign = m_Camera.transformMatrix[1][1] < 0.0f ? -1.0f : 1.0f;
		m_Camera.yaw += m_CameraLookSpeed * yawSign * mouseX;
		m_Camera.pitch += m_CameraLookSpeed * mouseY;
		m_Camera.pitch = std::min(m_Camera.pitch, glm::radians(90.0f));
		m_Camera.pitch = std::max(m_Camera.pitch, glm::radians(-90.0f));

		// Translation
		const glm::vec3 cameraRight = { m_Camera.transformMatrix[0][0], m_Camera.transformMatrix[0][1], m_Camera.transformMatrix[0][2] };
		const glm::vec3 cameraUp = { m_Camera.transformMatrix[1][0], m_Camera.transformMatrix[1][1], m_Camera.transformMatrix[1][2] };
		const glm::vec3 cameraForward = { m_Camera.transformMatrix[2][0], m_Camera.transformMatrix[2][1], m_Camera.transformMatrix[2][2] };

		glm::vec3 translation = glm::vec3(m_Camera.transformMatrix[3][0], m_Camera.transformMatrix[3][1], m_Camera.transformMatrix[3][2]);
		translation += dt * m_CameraMoveSpeed * Input::GetAxis1D(Input::KeyCode_D, Input::KeyCode_A) * cameraRight;
		translation += dt * m_CameraMoveSpeed * Input::GetAxis1D(Input::KeyCode_Space, Input::KeyCode_LeftShift) * cameraUp;
		translation += dt * m_CameraMoveSpeed * Input::GetAxis1D(Input::KeyCode_W, Input::KeyCode_S) * cameraForward;

		// Update camera matrix
		m_Camera.transformMatrix = glm::translate(glm::identity<glm::mat4>(), translation);
		m_Camera.transformMatrix = m_Camera.transformMatrix * glm::mat4_cast(glm::quat(glm::vec3(m_Camera.pitch, m_Camera.yaw, 0.0f)));
		m_Camera.viewMatrix = glm::inverse(m_Camera.transformMatrix);
	}
}

Camera CameraController::GetCamera() const
{
	return m_Camera;
}

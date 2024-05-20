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
		glm::vec3 rotation = {};
		float mouseX = Input::GetMouseRelX();
		float mouseY = Input::GetMouseRelY();

		float yawSign = m_Camera.viewMatrix[1][1] < 0.0f ? -1.0f : 1.0f;
		rotation.y -= m_CameraLookSpeed * yawSign * mouseX;
		rotation.x -= m_CameraLookSpeed * mouseY;
		rotation.x = std::min(rotation.x, glm::radians(90.0f));
		rotation.x = std::max(rotation.x, glm::radians(-90.0f));

		// Translation
		const glm::vec3 cameraRight = { m_Camera.invViewMatrix[0][0], m_Camera.invViewMatrix[0][1], m_Camera.invViewMatrix[0][2] };
		const glm::vec3 cameraUp = { m_Camera.invViewMatrix[1][0], m_Camera.invViewMatrix[1][1], m_Camera.invViewMatrix[1][2] };
		const glm::vec3 cameraForward = { m_Camera.invViewMatrix[2][0], m_Camera.invViewMatrix[2][1], m_Camera.invViewMatrix[2][2] };

		glm::vec3 translation = {};
		translation += dt * m_CameraMoveSpeed * Input::GetAxis1D(Input::KeyCode_A, Input::KeyCode_D) * cameraRight;
		translation += dt * m_CameraMoveSpeed * Input::GetAxis1D(Input::KeyCode_LeftShift, Input::KeyCode_Space) * cameraUp;
		translation += dt * m_CameraMoveSpeed * Input::GetAxis1D(Input::KeyCode_S, Input::KeyCode_W) * cameraForward;

		// Update camera matrix
		m_Camera.viewMatrix = glm::translate(m_Camera.viewMatrix, translation);
		m_Camera.viewMatrix = glm::mat4_cast(glm::quat(rotation)) * m_Camera.viewMatrix;
		m_Camera.invViewMatrix = glm::inverse(m_Camera.viewMatrix);
	}
}

Camera CameraController::GetCamera() const
{
	return m_Camera;
}

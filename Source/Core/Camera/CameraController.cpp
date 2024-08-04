#include "Pch.h"
#include "CameraController.h"
#include "Core/Input.h"

CameraController::CameraController(const Camera& Camera)
	: m_Camera(Camera)
{
}

void CameraController::Update(f32 DeltaTime)
{
	if (Input::IsMouseCaptured())
	{
		// Mouse wheel to increase/decrease camera move speed
		f32 ScrollY = Input::GetMouseScrollRelY();
		m_CameraMoveSpeed += 0.01f * ScrollY * std::max(std::sqrtf(m_CameraMoveSpeed), 0.005f);
		m_CameraMoveSpeed = std::max(m_CameraMoveSpeed, 0.0f);

		// Rotation
		f32 MouseX = Input::GetMouseRelX();
		f32 MouseY = Input::GetMouseRelY();

		f32 YawSign = m_Camera.TransformMatrix[1][1] < 0.0f ? -1.0f : 1.0f;
		m_Camera.Yaw += m_CameraLookSpeed * YawSign * MouseX;
		m_Camera.Pitch += m_CameraLookSpeed * MouseY;
		m_Camera.Pitch = std::min(m_Camera.Pitch, glm::radians(90.0f));
		m_Camera.Pitch = std::max(m_Camera.Pitch, glm::radians(-90.0f));

		// Translation
		const glm::vec3 CameraRight = { m_Camera.TransformMatrix[0][0], m_Camera.TransformMatrix[0][1], m_Camera.TransformMatrix[0][2] };
		const glm::vec3 CameraUp = { m_Camera.TransformMatrix[1][0], m_Camera.TransformMatrix[1][1], m_Camera.TransformMatrix[1][2] };
		const glm::vec3 CameraForward = { m_Camera.TransformMatrix[2][0], m_Camera.TransformMatrix[2][1], m_Camera.TransformMatrix[2][2] };

		glm::vec3 translation = glm::vec3(m_Camera.TransformMatrix[3][0], m_Camera.TransformMatrix[3][1], m_Camera.TransformMatrix[3][2]);
		translation += DeltaTime * m_CameraMoveSpeed * Input::GetAxis1D(Input::KeyCode_D, Input::KeyCode_A) * CameraRight;
		translation += DeltaTime * m_CameraMoveSpeed * Input::GetAxis1D(Input::KeyCode_Space, Input::KeyCode_LeftShift) * CameraUp;
		translation += DeltaTime * m_CameraMoveSpeed * Input::GetAxis1D(Input::KeyCode_W, Input::KeyCode_S) * CameraForward;

		// Update camera matrix
		m_Camera.TransformMatrix = glm::translate(glm::identity<glm::mat4>(), translation);
		m_Camera.TransformMatrix = m_Camera.TransformMatrix * glm::mat4_cast(glm::quat(glm::vec3(m_Camera.Pitch, m_Camera.Yaw, 0.0f)));
		m_Camera.ViewMatrix = glm::inverse(m_Camera.TransformMatrix);
	}
}

Camera CameraController::GetCamera() const
{
	return m_Camera;
}

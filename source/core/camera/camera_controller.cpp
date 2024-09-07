#include "pch.h"
#include "camera_controller.h"
#include "core/input.h"

camera_controller_t::camera_controller_t(const camera_t& camera)
	: m_camera(camera)
{
}

void camera_controller_t::update(f32 DeltaTime)
{
	if (input::is_mouse_captured())
	{
		// Mouse wheel to increase/decrease camera move speed
		f32 ScrollY = input::get_mouse_scroll_relY();
		m_camera_move_speed += 0.01f * ScrollY * std::max(std::sqrtf(m_camera_move_speed), 0.005f);
		m_camera_move_speed = std::max(m_camera_move_speed, 0.0f);

		// Rotation
		f32 MouseX = input::get_mouse_relX();
		f32 MouseY = input::get_mouse_relY();

		f32 YawSign = m_camera.transform_mat[1][1] < 0.0f ? -1.0f : 1.0f;
		m_camera.yaw += m_camera_look_speed * YawSign * MouseX;
		m_camera.pitch += m_camera_look_speed * MouseY;
		m_camera.pitch = std::min(m_camera.pitch, glm::radians(90.0f));
		m_camera.pitch = std::max(m_camera.pitch, glm::radians(-90.0f));

		// Translation
		const glm::vec3 CameraRight = { m_camera.transform_mat[0][0], m_camera.transform_mat[0][1], m_camera.transform_mat[0][2] };
		const glm::vec3 CameraUp = { m_camera.transform_mat[1][0], m_camera.transform_mat[1][1], m_camera.transform_mat[1][2] };
		const glm::vec3 CameraForward = { m_camera.transform_mat[2][0], m_camera.transform_mat[2][1], m_camera.transform_mat[2][2] };

		glm::vec3 translation = glm::vec3(m_camera.transform_mat[3][0], m_camera.transform_mat[3][1], m_camera.transform_mat[3][2]);
		translation += DeltaTime * m_camera_move_speed * input::get_axis_1D(input::KeyCode_D, input::KeyCode_A) * CameraRight;
		translation += DeltaTime * m_camera_move_speed * input::get_axis_1D(input::KeyCode_Space, input::KeyCode_LeftShift) * CameraUp;
		translation += DeltaTime * m_camera_move_speed * input::get_axis_1D(input::KeyCode_W, input::KeyCode_S) * CameraForward;

		// update camera matrix
		m_camera.transform_mat = glm::translate(glm::identity<glm::mat4>(), translation);
		m_camera.transform_mat = m_camera.transform_mat * glm::mat4_cast(glm::quat(glm::vec3(m_camera.pitch, m_camera.yaw, 0.0f)));
		m_camera.view_mat = glm::inverse(m_camera.transform_mat);
	}
}

camera_t camera_controller_t::get_camera() const
{
	return m_camera;
}

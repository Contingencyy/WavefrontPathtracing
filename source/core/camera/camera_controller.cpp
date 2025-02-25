#include "pch.h"
#include "camera_controller.h"
#include "camera.h"
#include "core/input.h"

namespace camera_controller
{

	void create(camera_controller_t& controller, camera_t* camera)
	{
		controller.camera = camera;
		controller.move_speed = 10.0f;
		controller.look_speed = 0.001f;
	}

	void destroy(camera_controller_t& controller)
	{
		controller.camera = nullptr;
		controller.move_speed = 0.0f;
		controller.look_speed = 0.0f;
	}

	void update(camera_controller_t& controller, float dt)
	{
		if (input::is_mouse_captured())
		{
			// Mouse wheel to increase/decrease camera move speed
			float ScrollY = input::get_mouse_scroll_relY();
			controller.move_speed += 0.01f * ScrollY * std::max(std::sqrtf(controller.move_speed), 0.005f);
			controller.move_speed = std::max(controller.move_speed, 0.0f);

			// Rotation
			float MouseX = input::get_mouse_relX();
			float MouseY = input::get_mouse_relY();

			float YawSign = controller.camera->inv_view_matrix[1][1] < 0.0f ? -1.0f : 1.0f;
			controller.camera->yaw += controller.look_speed * YawSign * MouseX;
			controller.camera->pitch += controller.look_speed * MouseY;
			controller.camera->pitch = std::min(controller.camera->pitch, glm::radians(90.0f));
			controller.camera->pitch = std::max(controller.camera->pitch, glm::radians(-90.0f));

			// Translation
			const glm::vec3 CameraRight = { controller.camera->inv_view_matrix[0][0], controller.camera->inv_view_matrix[0][1], controller.camera->inv_view_matrix[0][2] };
			const glm::vec3 CameraUp = { controller.camera->inv_view_matrix[1][0], controller.camera->inv_view_matrix[1][1], controller.camera->inv_view_matrix[1][2] };
			const glm::vec3 CameraForward = { controller.camera->inv_view_matrix[2][0], controller.camera->inv_view_matrix[2][1], controller.camera->inv_view_matrix[2][2] };

			glm::vec3 translation = glm::vec3(controller.camera->inv_view_matrix[3][0], controller.camera->inv_view_matrix[3][1], controller.camera->inv_view_matrix[3][2]);
			translation += dt * controller.move_speed * input::get_axis_1D(input::KeyCode_D, input::KeyCode_A) * CameraRight;
			translation += dt * controller.move_speed * input::get_axis_1D(input::KeyCode_Space, input::KeyCode_LeftShift) * CameraUp;
			translation += dt * controller.move_speed * input::get_axis_1D(input::KeyCode_W, input::KeyCode_S) * CameraForward;

			// update camera matrix
			controller.camera->inv_view_matrix = glm::translate(glm::identity<glm::mat4>(), translation);
			controller.camera->inv_view_matrix = controller.camera->inv_view_matrix * glm::mat4_cast(glm::quat(glm::vec3(controller.camera->pitch, controller.camera->yaw, 0.0f)));
			controller.camera->view_matrix = glm::inverse(controller.camera->inv_view_matrix);
		}
	}

}

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
			float scroll_y = input::get_mouse_scroll_rely();
			controller.move_speed += 0.01f * scroll_y * std::max(std::sqrtf(controller.move_speed), 0.005f);
			controller.move_speed = std::max(controller.move_speed, 0.0f);

			// Rotation
			float mouse_x = input::get_mouse_relx();
			float mouse_y = input::get_mouse_rely();

			float yaw_sign = controller.camera->inv_view_matrix[1][1] < 0.0f ? -1.0f : 1.0f;
			controller.camera->yaw += controller.look_speed * yaw_sign * mouse_x;
			controller.camera->pitch += controller.look_speed * mouse_y;
			controller.camera->pitch = std::min(controller.camera->pitch, glm::radians(90.0f));
			controller.camera->pitch = std::max(controller.camera->pitch, glm::radians(-90.0f));

			// Translation
			const glm::vec3 camera_right = { controller.camera->inv_view_matrix[0][0], controller.camera->inv_view_matrix[0][1], controller.camera->inv_view_matrix[0][2] };
			const glm::vec3 camera_up = { controller.camera->inv_view_matrix[1][0], controller.camera->inv_view_matrix[1][1], controller.camera->inv_view_matrix[1][2] };
			const glm::vec3 camera_forward = { controller.camera->inv_view_matrix[2][0], controller.camera->inv_view_matrix[2][1], controller.camera->inv_view_matrix[2][2] };

			glm::vec3 translation = glm::vec3(controller.camera->inv_view_matrix[3][0], controller.camera->inv_view_matrix[3][1], controller.camera->inv_view_matrix[3][2]);
			translation += dt * controller.move_speed * input::get_axis_1d(input::KEYCODE_D, input::KEYCODE_A) * camera_right;
			translation += dt * controller.move_speed * input::get_axis_1d(input::KEYCODE_SPACE, input::KEYCODE_LEFT_SHIFT) * camera_up;
			translation += dt * controller.move_speed * input::get_axis_1d(input::KEYCODE_W, input::KEYCODE_S) * camera_forward;

			// update camera matrix
			controller.camera->inv_view_matrix = glm::translate(glm::identity<glm::mat4>(), translation);
			controller.camera->inv_view_matrix = controller.camera->inv_view_matrix * glm::mat4_cast(glm::quat(glm::vec3(controller.camera->pitch, controller.camera->yaw, 0.0f)));
			controller.camera->view_matrix = glm::inverse(controller.camera->inv_view_matrix);
		}
	}

}

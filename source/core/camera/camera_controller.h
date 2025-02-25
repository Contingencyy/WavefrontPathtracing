#pragma once

struct camera_t;

struct camera_controller_t
{
	camera_t* camera;
	float move_speed;
	float look_speed;
};

namespace camera_controller
{

	void create(camera_controller_t& controller, camera_t* camera);
	void destroy(camera_controller_t& controller);

	void update(camera_controller_t& controller, float dt);

}

#pragma once
#include "camera.h"

class camera_controller_t
{
public:
	camera_controller_t() = default;
	camera_controller_t(const camera_t& Camera);

	void update(f32 DeltaTime);

	camera_t get_camera() const;
	
private:
	camera_t m_camera;

	f32 m_camera_move_speed = 10.0f;
	f32 m_camera_look_speed = 0.001f;

};

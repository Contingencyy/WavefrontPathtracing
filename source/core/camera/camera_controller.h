#pragma once
#include "camera.h"

class camera_controller_t
{
public:
	camera_controller_t() = default;
	camera_controller_t(const camera_t& Camera);

	void update(float DeltaTime);

	camera_t get_camera() const;
	
private:
	camera_t m_camera;

	float m_camera_move_speed = 10.0f;
	float m_camera_look_speed = 0.001f;

};

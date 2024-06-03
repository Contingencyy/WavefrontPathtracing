#pragma once
#include "Camera.h"

class CameraController
{
public:
	CameraController() = default;
	CameraController(const Camera& camera);

	void Update(float dt);

	Camera GetCamera() const;
	
private:
	Camera m_Camera;

	float m_CameraMoveSpeed = 10.0f;
	float m_CameraLookSpeed = 0.001f;

};

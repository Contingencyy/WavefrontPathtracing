#pragma once
#include "Camera.h"

class CameraController
{
public:
	CameraController() = default;
	CameraController(const Camera& Camera);

	void Update(f32 DeltaTime);

	Camera GetCamera() const;
	
private:
	Camera m_Camera;

	f32 m_CameraMoveSpeed = 10.0f;
	f32 m_CameraLookSpeed = 0.001f;

};

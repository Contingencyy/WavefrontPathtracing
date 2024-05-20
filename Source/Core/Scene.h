#pragma once
#include "Renderer/CameraController.h"
#include "Renderer/RaytracingTypes.h"

class Scene
{
public:
	Scene();

	void Update(float dt);
	void Render();
	bool Intersect(Ray& ray);

private:
	CameraController m_CameraController;

	std::vector<Primitive> m_SceneNodes;

};

#pragma once
#include "Core/Camera/CameraController.h"
#include "Renderer/RaytracingTypes.h"

struct Object
{
	Primitive prim;
	Material mat;
};

class Scene
{
public:
	Scene();

	void Update(float dt);
	void Render();
	HitSurfaceData Intersect(Ray& ray);

private:
	CameraController m_CameraController;

	std::vector<Object> m_SceneNodes;

};

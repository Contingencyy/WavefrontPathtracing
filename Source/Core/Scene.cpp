#include "Pch.h"
#include "Scene.h"
#include "Renderer/CPUPathtracer.h"

#include "Renderer/RaytracingUtils.h"

Scene::Scene()
{
	m_CameraController = CameraController(Camera(
		glm::vec3(0.0f), // Eye position
		glm::vec3(0.0f, 0.0f, 1.0f), // Look at position
		60.0f // Vertical FOV in degrees
	));

	m_SceneNodes.push_back({ .type = PrimitiveType_Sphere, .sphere = {.center = glm::vec3(0.0f, 0.0f, 10.0f), .radiusSquared = 16.0f } });
}

void Scene::Update(float dt)
{
	m_CameraController.Update(dt);
}

void Scene::Render()
{
	CPUPathtracer::BeginScene(m_CameraController.GetCamera());
	CPUPathtracer::Render(this);
	CPUPathtracer::EndScene();
}

bool Scene::Intersect(Ray& ray)
{
	for (const auto& sceneNode : m_SceneNodes)
	{
		switch (sceneNode.type)
		{
		case PrimitiveType_Triangle: return RTUtil::Intersect(sceneNode.tri, ray);
		case PrimitiveType_Sphere: return RTUtil::Intersect(sceneNode.sphere, ray);
		case PrimitiveType_Plane: return RTUtil::Intersect(sceneNode.plane, ray);
		case PrimitiveType_AABB: return RTUtil::Intersect(sceneNode.aabb, ray);
		default: FATAL_ERROR("Scene::Intersect", "Invalid primitive type"); return false;
		}
	}
}

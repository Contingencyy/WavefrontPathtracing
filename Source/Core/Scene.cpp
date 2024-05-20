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

	m_SceneNodes.push_back({ .type = PrimitiveType_Sphere, .sphere = { .center = glm::vec3(0.0f, 0.0f, 10.0f), .radiusSquared = 16.0f } });
	m_SceneNodes.push_back({ .type = PrimitiveType_Plane, .plane = { .point = glm::vec3(0.0f, 0.0f, 0.0f), .normal = glm::vec3(0.0f, 1.0f, 0.0f)}});
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
	bool hit = false;

	for (const auto& sceneNode : m_SceneNodes)
	{
		switch (sceneNode.type)
		{
		case PrimitiveType_Triangle: hit = RTUtil::Intersect(sceneNode.tri, ray); break;
		case PrimitiveType_Sphere: hit = RTUtil::Intersect(sceneNode.sphere, ray); break;
		case PrimitiveType_Plane: hit = RTUtil::Intersect(sceneNode.plane, ray); break;
		case PrimitiveType_AABB: hit = RTUtil::Intersect(sceneNode.aabb, ray); break;
		default: FATAL_ERROR("Scene::Intersect", "Invalid primitive type"); break;
		}
	}

	return hit;
}

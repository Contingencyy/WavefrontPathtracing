#pragma once
#include "Core/Camera/CameraController.h"
#include "Renderer/RendererFwd.h"
#include "Renderer/RaytracingUtils.h"

struct SceneObject
{
	RenderMeshHandle renderMeshHandle;
	glm::mat4 transform;
	Material material;

	SceneObject(RenderMeshHandle meshHandle, const Material& mat,
		const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale)
	{
		renderMeshHandle = meshHandle;

		transform = glm::translate(glm::identity<glm::mat4>(), pos);
		transform = transform * glm::mat4_cast(glm::quat(glm::radians(rot)));
		transform = glm::scale(transform, scale);

		material = mat;
	}
};

class Scene
{
public:
	Scene();

	void Update(float dt);
	void Render();

	void RenderUI();

private:
	CameraController m_CameraController;

	std::vector<SceneObject> m_SceneObjects;

};

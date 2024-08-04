#pragma once
#include "Core/Camera/CameraController.h"
#include "Renderer/RendererFwd.h"
#include "Renderer/RaytracingUtils.h"

struct TextureAsset;
struct SceneAsset;

struct SceneObject
{
	RenderMeshHandle RMeshHandle;
	glm::mat4 Transform;
	Material Mat;
};

class Scene
{
public:
	void Init();
	void Destroy();

	void Update(f32 DeltaTime);
	void Render();

	void RenderUI();

private:
	void CreateSceneObject(RenderMeshHandle RMeshHandle, const Material& Mat,
		const glm::vec3& Pos, const glm::vec3& Rot, const glm::vec3& Scale);

private:
	MemoryArena m_Arena;

	CameraController m_CameraController;

	// TODO: Free-list for scene objects
	u32 m_SceneObjectCount;
	u32 m_SceneObjectAt;
	SceneObject* m_SceneObjects;

	TextureAsset* m_HDREnvAsset;
	SceneAsset* m_DragonAsset;

};

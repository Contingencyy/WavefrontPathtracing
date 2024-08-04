#include "Pch.h"
#include "Scene.h"
#include "Renderer/CPUPathtracer.h"

// TODO: Remove and implement a proper asset manager
#include "Core/Assets/AssetLoader.h"

#include "imgui/imgui.h"

void Scene::Init()
{
	// Scene objects
	m_SceneObjects = ARENA_ALLOC_ARRAY_ZERO(&m_Arena, SceneObject, 100);
	m_SceneObjectCount = 100;
	m_SceneObjectAt = 0;

	// Camera Controller
	m_CameraController = CameraController(Camera(
		glm::vec3(0.0f, 10.0f, 0.0f), // Eye Position
		glm::vec3(0.0f, 10.0f, 1.0f), // Look at Position
		60.0f // Vertical FOV in degrees
	));

	// Plane
	Vertex PlaneVertices[4] = {};
	PlaneVertices[0].Position = glm::vec3(-1.0f, 0.0f, 1.0f);
	PlaneVertices[1].Position = glm::vec3(1.0f, 0.0f, 1.0f);
	PlaneVertices[2].Position = glm::vec3(1.0f, 0.0f, -1.0f);
	PlaneVertices[3].Position = glm::vec3(-1.0f, 0.0f, -1.0f);
	PlaneVertices[0].Normal = PlaneVertices[1].Normal = PlaneVertices[2].Normal = PlaneVertices[3].Normal = glm::vec3(0.0f, 1.0f, 0.0f);

	u32 PlaneIndices[6] = { 0, 1, 2, 2, 3, 0 };
	RenderMeshHandle RMeshHandlePlane = CPUPathtracer::CreateMesh(PlaneVertices, ARRAY_SIZE(PlaneVertices), PlaneIndices, ARRAY_SIZE(PlaneIndices));

	Material PlaneMaterial = Material::MakeDiffuse(glm::vec3(1.0f));
	CreateSceneObject(RMeshHandlePlane, PlaneMaterial, glm::vec3(0.0f, 0.0f, 80.0f), glm::vec3(0.0f), glm::vec3(120.0f));

	// HDR environment map
	//m_HDREnvAsset = AssetLoader::LoadImageHDR(&m_Arena, "Assets/Textures/HDR_Env_Victorian_Hall.hdr");
	//m_HDREnvAsset = AssetLoader::LoadImageHDR(&m_Arena, "Assets/Textures/HDR_Env_St_Peters_Square_Night.hdr");
	m_HDREnvAsset = AssetLoader::LoadImageHDR(&m_Arena, "Assets/Textures/HDR_Env_Country_Club.hdr");

	// Dragon
	m_DragonAsset = AssetLoader::LoadGLTF(&m_Arena, "Assets/GLTF/Dragon/DragonAttenuation.gltf");
	// Dragon 1
	//Material DragonMaterial = Material::MakeRefractive(glm::vec3(1.0f), 0.0f, 1.0f, 1.517f, glm::vec3(0.2f, 0.95f, 0.95f));
	Material DragonMaterial = Material::MakeDiffuse(glm::vec3(0.9f, 0.1f, 0.05f));
	CreateSceneObject(m_DragonAsset->RMeshHandles[1], DragonMaterial, glm::vec3(-15.0f, 0.0f, 40.0f), glm::vec3(90.0f, 180.0f, 0.0f), glm::vec3(1.0f));

	// Dragon 2
	DragonMaterial = Material::MakeDiffuse(glm::vec3(0.05f, 0.1f, 0.9f));
	CreateSceneObject(m_DragonAsset->RMeshHandles[1], DragonMaterial, glm::vec3(15.0f, 0.0f, 40.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(2.0f));

	// Dragon 3
	DragonMaterial = Material::MakeDiffuse(glm::vec3(0.1f, 0.9f, 0.1f));
	CreateSceneObject(m_DragonAsset->RMeshHandles[1], DragonMaterial, glm::vec3(-30.0f, 0.0f, 70.0f), glm::vec3(90.0f, 180.0f, 0.0f), glm::vec3(3.0f));

	// Dragon 4
	DragonMaterial = Material::MakeDiffuse(glm::vec3(0.9f, 0.9f, 0.1f));
	CreateSceneObject(m_DragonAsset->RMeshHandles[1], DragonMaterial, glm::vec3(30.0f, 0.0f, 70.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(4.0f));

	// Dragon 5
	DragonMaterial = Material::MakeSpecular(glm::vec3(0.8f, 0.7f, 0.2f), 1.0f);
	CreateSceneObject(m_DragonAsset->RMeshHandles[1], DragonMaterial, glm::vec3(0.0f, 0.0f, 120.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(5.0f));
}

void Scene::Destroy()
{
	ARENA_RELEASE(&m_Arena);
}

void Scene::Update(f32 DeltaTime)
{
	m_CameraController.Update(DeltaTime);
}

void Scene::Render()
{
	CPUPathtracer::BeginScene(m_CameraController.GetCamera(), m_HDREnvAsset->RTextureHandle);

	// Submit every object that needs to be rendered
	for (u32 i = 0; i < m_SceneObjectAt; ++i)
	{
		const SceneObject* object = &m_SceneObjects[i];
		CPUPathtracer::SubmitMesh(object->RMeshHandle, object->Transform, object->Mat);
	}

	CPUPathtracer::Render();
	CPUPathtracer::EndScene();
}

void Scene::RenderUI()
{
}

void Scene::CreateSceneObject(RenderMeshHandle RMeshHandle, const Material& Mat, const glm::vec3& Pos, const glm::vec3& Rot, const glm::vec3& Scale)
{
	ASSERT(m_SceneObjectAt < m_SceneObjectCount);
	SceneObject* Object = &m_SceneObjects[m_SceneObjectAt];

	Object->RMeshHandle = RMeshHandle;
	Object->Mat = Mat;

	Object->Transform = glm::translate(glm::identity<glm::mat4>(), Pos);
	Object->Transform = Object->Transform * glm::mat4_cast(glm::quat(glm::radians(Rot)));
	Object->Transform = Object->Transform * glm::scale(glm::identity<glm::mat4>(), Scale);

	m_SceneObjectAt++;
}

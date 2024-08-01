#include "Pch.h"
#include "Scene.h"
#include "Renderer/CPUPathtracer.h"

// TODO: Remove
#include "Core/Assets/AssetLoader.h"

#include "imgui/imgui.h"

static TextureAsset hdrEnvironment;

Scene::Scene()
{
	// Camera Controller
	m_CameraController = CameraController(Camera(
		glm::vec3(0.0f, 5.0f, 0.0f), // Eye position
		glm::vec3(0.0f, 5.0f, 1.0f), // Look at position
		60.0f // Vertical FOV in degrees
	));

	//// Plane
	//Primitive plane = {
	//	.type = PrimitiveType_Plane,
	//	.plane = {
	//		.point = glm::vec3(0.0f),
	//		.normal = glm::vec3(0.0f, 1.0f, 0.0f)
	//	}
	//};
	////Material planeMaterial = Material::MakeSpecular(glm::vec3(0.2f), 0.5f);
	//Material planeMaterial = Material::MakeDiffuse(glm::vec3(1.0f));
	//m_SceneObjects.emplace_back(plane, planeMaterial);

	//hdrEnvironment = AssetLoader::LoadImageHDR("Assets/Textures/HDR_Env_Victorian_Hall.hdr");
	hdrEnvironment = AssetLoader::LoadImageHDR("Assets/Textures/HDR_Env_St_Peters_Square_Night.hdr");
	static SceneAsset dragonGLTF = AssetLoader::LoadGLTF("Assets/GLTF/Dragon/DragonAttenuation.gltf");
	// Dragon 1
	//Material dragonMaterial = Material::MakeRefractive(glm::vec3(1.0f), 0.0f, 1.0f, 1.517f, glm::vec3(0.2f, 0.95f, 0.95f));
	Material dragonMaterial = Material::MakeDiffuse(glm::vec3(0.9f, 0.1f, 0.05f));
	m_SceneObjects.emplace_back(dragonGLTF.renderMeshHandles[1], dragonMaterial, glm::vec3(-8.0f, 0.0f, 20.0f), glm::vec3(90.0f, 180.0f, 0.0f), glm::vec3(0.1f));

	// Dragon 2
	dragonMaterial = Material::MakeDiffuse(glm::vec3(0.05f, 0.1f, 0.9f));
	m_SceneObjects.emplace_back(dragonGLTF.renderMeshHandles[1], dragonMaterial, glm::vec3(8.0f, 0.0f, 20.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(0.4f));

	// Dragon 3
	dragonMaterial = Material::MakeDiffuse(glm::vec3(0.1f, 0.9f, 0.1f));
	m_SceneObjects.emplace_back(dragonGLTF.renderMeshHandles[1], dragonMaterial, glm::vec3(-8.0f, 0.0f, 30.0f), glm::vec3(90.0f, 180.0f, 0.0f), glm::vec3(0.7f));

	// Dragon 4
	dragonMaterial = Material::MakeDiffuse(glm::vec3(0.9f, 0.9f, 0.1f));
	m_SceneObjects.emplace_back(dragonGLTF.renderMeshHandles[1], dragonMaterial, glm::vec3(8.0f, 0.0f, 30.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(1.0f));

	// Dragon 5
	dragonMaterial = Material::MakeSpecular(glm::vec3(1.0f), 1.0f);
	m_SceneObjects.emplace_back(dragonGLTF.renderMeshHandles[1], dragonMaterial, glm::vec3(0.0f, 0.0f, 100.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(10.0f));
}

void Scene::Update(float dt)
{
	m_CameraController.Update(dt);
}

void Scene::Render()
{
	CPUPathtracer::BeginScene(m_CameraController.GetCamera(), hdrEnvironment.renderTextureHandle);

	// Submit every object that needs to be rendered
	for (size_t i = 0; i < m_SceneObjects.size(); ++i)
	{
		const SceneObject& object = m_SceneObjects[i];

		CPUPathtracer::SubmitMesh(object.renderMeshHandle, object.transform, object.material);
	}

	CPUPathtracer::Render();
	CPUPathtracer::EndScene();
}

void Scene::RenderUI()
{
}

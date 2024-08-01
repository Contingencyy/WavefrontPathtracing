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

	std::vector<Vertex> planeVertices;
	planeVertices.emplace_back(glm::vec3(-1.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	planeVertices.emplace_back(glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	planeVertices.emplace_back(glm::vec3(1.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	planeVertices.emplace_back(glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	std::vector<uint32_t> planeIndices =
	{
		0, 1, 2, 2, 3, 0
	};
	RenderMeshHandle planeMeshHandle = CPUPathtracer::CreateMesh(planeVertices, planeIndices);
	Material planeMaterial = Material::MakeDiffuse(glm::vec3(1.0f));
	m_SceneObjects.emplace_back(planeMeshHandle, planeMaterial, glm::vec3(0.0f, 0.0f, 80.0f), glm::vec3(0.0f), glm::vec3(120.0f));

	//hdrEnvironment = AssetLoader::LoadImageHDR("Assets/Textures/HDR_Env_Victorian_Hall.hdr");
	//hdrEnvironment = AssetLoader::LoadImageHDR("Assets/Textures/HDR_Env_St_Peters_Square_Night.hdr");
	hdrEnvironment = AssetLoader::LoadImageHDR("Assets/Textures/HDR_Env_Country_Club.hdr");
	static SceneAsset dragonGLTF = AssetLoader::LoadGLTF("Assets/GLTF/Dragon/DragonAttenuation.gltf");
	// Dragon 1
	//Material dragonMaterial = Material::MakeRefractive(glm::vec3(1.0f), 0.0f, 1.0f, 1.517f, glm::vec3(0.2f, 0.95f, 0.95f));
	Material dragonMaterial = Material::MakeDiffuse(glm::vec3(0.9f, 0.1f, 0.05f));
	m_SceneObjects.emplace_back(dragonGLTF.renderMeshHandles[1], dragonMaterial, glm::vec3(-8.0f, 0.0f, 20.0f), glm::vec3(90.0f, 180.0f, 0.0f), glm::vec3(0.4f));

	// Dragon 2
	dragonMaterial = Material::MakeDiffuse(glm::vec3(0.05f, 0.1f, 0.9f));
	m_SceneObjects.emplace_back(dragonGLTF.renderMeshHandles[1], dragonMaterial, glm::vec3(8.0f, 0.0f, 20.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(0.6f));

	// Dragon 3
	dragonMaterial = Material::MakeDiffuse(glm::vec3(0.1f, 0.9f, 0.1f));
	m_SceneObjects.emplace_back(dragonGLTF.renderMeshHandles[1], dragonMaterial, glm::vec3(-8.0f, 0.0f, 30.0f), glm::vec3(90.0f, 180.0f, 0.0f), glm::vec3(0.8f));

	// Dragon 4
	dragonMaterial = Material::MakeDiffuse(glm::vec3(0.9f, 0.9f, 0.1f));
	m_SceneObjects.emplace_back(dragonGLTF.renderMeshHandles[1], dragonMaterial, glm::vec3(8.0f, 0.0f, 30.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(1.0f));

	// Dragon 5
	dragonMaterial = Material::MakeSpecular(glm::vec3(0.8f, 0.7f, 0.2f), 1.0f);
	m_SceneObjects.emplace_back(dragonGLTF.renderMeshHandles[1], dragonMaterial, glm::vec3(0.0f, 0.0f, 60.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(5.0f));
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

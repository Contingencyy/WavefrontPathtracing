#include "Pch.h"
#include "Scene.h"
#include "Renderer/CPUPathtracer.h"

#include "Renderer/RaytracingUtils.h"
#include "Core/Assets/AssetLoader.h"

Scene::Scene()
{
	// Camera Controller
	m_CameraController = CameraController(Camera(
		glm::vec3(0.0f, 2.0f, 0.0f), // Eye position
		glm::vec3(0.0f, 2.0f, 1.0f), // Look at position
		60.0f // Vertical FOV in degrees
	));

	// Plane
	Primitive plane = {
		.type = PrimitiveType_Plane,
		.plane = {
			.point = glm::vec3(0.0f),
			.normal = glm::vec3(0.0f, 1.0f, 0.0f)
		}
	};
	Material planeMaterial = Material::MakeSpecular(glm::vec3(0.5f), 0.5f);
	m_SceneNodes.emplace_back(plane, planeMaterial);

	// Red sphere
	Primitive redSphere = {
		.type = PrimitiveType_Sphere,
		.sphere = {
			.center = glm::vec3(2.2f, 2.0f, 8.0f),
			.radiusSquared = 4.0f
		}
	};
	Material redSphereMaterial = Material::MakeDiffuse(glm::vec3(0.9f, 0.1f, 0.1f));
	m_SceneNodes.emplace_back(redSphere, redSphereMaterial);

	// Blue sphere
	Primitive blueSphere = {
		.type = PrimitiveType_Sphere,
		.sphere = {
			.center = glm::vec3(-2.2f, 2.0f, 8.0f),
			.radiusSquared = 4.0f
		}
	};
	Material blueSphereMaterial = Material::MakeDiffuse(glm::vec3(0.1f, 0.1f, 0.9f));
	m_SceneNodes.emplace_back(blueSphere, blueSphereMaterial);

	// Green AABB
	Primitive greenAABB = {
		.type = PrimitiveType_AABB,
		.aabb = {
			.pmin = glm::vec3(-3.0f, 0.0f, 12.0f),
			.pmax = glm::vec3(3.0f, 6.0f, 18.0f)
		}
	};
	Material greenAABBMaterial = Material::MakeSpecular(glm::vec3(0.1f, 0.9f, 0.1f), 0.3f);
	m_SceneNodes.emplace_back(greenAABB, greenAABBMaterial);

	// Glass sphere
	Primitive glassSphere = {
		.type = PrimitiveType_Sphere,
		.sphere = {
			.center = glm::vec3(-12.0f, 8.0f, 20.0f),
			.radiusSquared = 16.0f
		}
	};
	Material glassSphereMaterial = Material::MakeRefractive(glm::vec3(0.8f), 0.0f, 1.0f, 1.517f, glm::vec3(0.1f, 0.1f, 0.9f));
	m_SceneNodes.emplace_back(glassSphere, glassSphereMaterial);

	// Dragon
	MeshAsset dragonMesh = AssetLoader::LoadGLTF("Assets/GLTF/Dragon/DragonAttenuation.gltf");
	Material dragonMaterial = Material::MakeRefractive(glm::vec3(1.0f), 0.0f, 1.0f, 1.517f, glm::vec3(0.2f, 0.8f, 0.8f));
	m_SceneNodes.emplace_back(dragonMesh, dragonMaterial);
}

void Scene::Update(float dt)
{
	m_CameraController.Update(dt);
}

void Scene::Render()
{
	CPUPathtracer::BeginScene(m_CameraController.GetCamera());
	CPUPathtracer::Render(*this);
	CPUPathtracer::EndScene();
}

HitSurfaceData Scene::TraceRay(Ray& ray) const
{
	HitSurfaceData hitInfo = {};
	uint32_t primitiveID = ~0u;

	for (uint32_t i = 0; i < m_SceneNodes.size(); ++i)
	{
		if (m_SceneNodes[i].hasBVH)
		{
			const BVH& objectBVH = m_SceneNodes[i].boundingVolumeHierarchy;
			primitiveID = objectBVH.TraceRay(ray);

			if (primitiveID != ~0u)
				hitInfo.objIdx = i;
		}
		else
		{
			const Primitive& objectPrim = m_SceneNodes[i].primitive;

			switch (objectPrim.type)
			{
			case PrimitiveType_Triangle: if (RTUtil::Intersect(objectPrim.tri, ray)) { hitInfo.objIdx = i; }; break;
			case PrimitiveType_Sphere: if (RTUtil::Intersect(objectPrim.sphere, ray)) { hitInfo.objIdx = i; }; break;
			case PrimitiveType_Plane: if (RTUtil::Intersect(objectPrim.plane, ray)) { hitInfo.objIdx = i; }; break;
			case PrimitiveType_AABB: if (RTUtil::Intersect(objectPrim.aabb, ray)) { hitInfo.objIdx = i; }; break;
			default: FATAL_ERROR("Scene::Intersect", "Invalid primitive type"); break;
			}
		}
	}

	if (hitInfo.objIdx != ~0u)
	{
		hitInfo.pos = ray.origin + ray.dir * ray.t;
		hitInfo.objMat = m_SceneNodes[hitInfo.objIdx].material;

		if (m_SceneNodes[hitInfo.objIdx].hasBVH)
		{
			const BVH& hitBVH = m_SceneNodes[hitInfo.objIdx].boundingVolumeHierarchy;
			hitInfo.normal = RTUtil::GetHitNormal(hitBVH.GetTriangle(primitiveID), hitInfo.pos);
		}
		else
		{
			const Primitive& hitPrim = m_SceneNodes[hitInfo.objIdx].primitive;

			switch (hitPrim.type)
			{
			case PrimitiveType_Triangle: hitInfo.normal = RTUtil::GetHitNormal(hitPrim.tri, hitInfo.pos); break;
			case PrimitiveType_Sphere: hitInfo.normal = RTUtil::GetHitNormal(hitPrim.sphere, hitInfo.pos); break;
			case PrimitiveType_Plane: hitInfo.normal = RTUtil::GetHitNormal(hitPrim.plane, hitInfo.pos); break;
			case PrimitiveType_AABB: hitInfo.normal = RTUtil::GetHitNormal(hitPrim.aabb, hitInfo.pos); break;
			default: FATAL_ERROR("Scene::Intersect", "Invalid primitive type"); break;
			}
		}
	}

	return hitInfo;
}

#include "Pch.h"
#include "Scene.h"
#include "Renderer/CPUPathtracer.h"

#include "Renderer/RaytracingUtils.h"
#include "Core/Assets/AssetLoader.h"

#include "imgui/imgui.h"

Scene::Scene()
{
	// Camera Controller
	m_CameraController = CameraController(Camera(
		glm::vec3(0.0f, 5.0f, 0.0f), // Eye position
		glm::vec3(0.0f, 5.0f, 1.0f), // Look at position
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
	//Material planeMaterial = Material::MakeSpecular(glm::vec3(0.2f), 0.5f);
	Material planeMaterial = Material::MakeDiffuse(glm::vec3(1.0f));
	m_SceneNodes.emplace_back(plane, planeMaterial);

	static MeshAsset dragonMesh = AssetLoader::LoadGLTF("Assets/GLTF/Dragon/DragonAttenuation.gltf");
	dragonMesh.boundingVolumeHierarchy.Build(dragonMesh.vertices, dragonMesh.indices, BVH::BuildOptions{});

	// Dragon 1
	//Material dragonMaterial = Material::MakeRefractive(glm::vec3(1.0f), 0.0f, 1.0f, 1.517f, glm::vec3(0.2f, 0.95f, 0.95f));
	Material dragonMaterial = Material::MakeDiffuse(glm::vec3(0.9f, 0.1f, 0.05f));
	m_SceneNodes.emplace_back(dragonMesh, dragonMaterial, glm::vec3(-8.0f, 0.0f, 15.0f), glm::vec3(90.0f, 180.0f, 0.0f), glm::vec3(1.0f));

	// Dragon 2
	dragonMaterial = Material::MakeDiffuse(glm::vec3(0.05f, 0.1f, 0.9f));
	m_SceneNodes.emplace_back(dragonMesh, dragonMaterial, glm::vec3(8.0f, 0.0f, 15.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(1.0f));

	// Dragon 3
	dragonMaterial = Material::MakeDiffuse(glm::vec3(0.1f, 0.9f, 0.1f));
	m_SceneNodes.emplace_back(dragonMesh, dragonMaterial, glm::vec3(-8.0f, 0.0f, 30.0f), glm::vec3(90.0f, 180.0f, 0.0f), glm::vec3(1.0f));

	// Dragon 4
	dragonMaterial = Material::MakeDiffuse(glm::vec3(0.9f, 0.1f, 0.9f));
	m_SceneNodes.emplace_back(dragonMesh, dragonMaterial, glm::vec3(8.0f, 0.0f, 30.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(1.0f));

	// Get all BLAS instances and build the scene TLAS
	std::vector<BVHInstance> blasInstances;
	for (size_t i = 0; i < m_SceneNodes.size(); ++i)
	{
		const SceneObject& object = m_SceneNodes[i];
		if (object.hasBVH)
		{
			blasInstances.push_back(object.bvhInstance);
		}
	}

	m_SceneTLAS.Build(blasInstances);
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

void Scene::RenderUI()
{
	if (ImGui::Begin("Scene"))
	{
	}

	ImGui::End();
}

HitSurfaceData Scene::TraceRay(Ray& ray) const
{
	HitSurfaceData hitInfo = {};

	// Trace ray through the scene TLAS
	TLAS::TraceResult traceResult = m_SceneTLAS.TraceRay(ray);

	if (traceResult.instanceIndex != ~0u &&
		traceResult.primitiveIndex != ~0u)
	{
		hitInfo.pos = ray.origin + ray.dir * ray.t;
		// + 1 because we also have a plane there at objIdx 0, primitive only acceleration structures should be a thing probably?
		hitInfo.objIdx = traceResult.instanceIndex + 1;
		hitInfo.objMat = m_SceneNodes[hitInfo.objIdx].material;
		hitInfo.normal = m_SceneTLAS.GetNormal(traceResult.instanceIndex, traceResult.primitiveIndex);
	}

	//for (uint32_t i = 0; i < m_SceneNodes.size(); ++i)
	//{
	//	const SceneObject& object = m_SceneNodes[i];

	//	if (object.hasBVH)
	//	{
	//		object.bvhInstance.TraceRay(ray);

	//		/*if (bvhPrimitiveID != ~0u)
	//		{
	//			hitInfo.primitiveID = bvhPrimitiveID;
	//			hitInfo.objIdx = i;
	//		}*/
	//	}
	//	else
	//	{
	//		switch (object.primitive.type)
	//		{
	//		case PrimitiveType_Triangle: if (RTUtil::Intersect(object.primitive.tri, ray)) { hitInfo.objIdx = i; }; break;
	//		case PrimitiveType_Sphere: if (RTUtil::Intersect(object.primitive.sphere, ray)) { hitInfo.objIdx = i; }; break;
	//		case PrimitiveType_Plane: if (RTUtil::Intersect(object.primitive.plane, ray)) { hitInfo.objIdx = i; }; break;
	//		case PrimitiveType_AABB: if (RTUtil::Intersect(object.primitive.aabb, ray)) { hitInfo.objIdx = i; }; break;
	//		default: FATAL_ERROR("Scene::Intersect", "Invalid primitive type"); break;
	//		}
	//	}
	//}

	//if (hitInfo.objIdx != ~0u)
	//{
	//	const SceneObject& object = m_SceneNodes[hitInfo.objIdx];

	//	hitInfo.pos = ray.origin + ray.dir * ray.t;
	//	hitInfo.objMat = object.material;

	//	if (object.hasBVH)
	//	{
	//		if (hitInfo.primitiveID == ~0u)
	//			FATAL_ERROR("Scene::TraceRay", "Ray has intersected with BVH, but primitiveID is invalid");
	//		//hitInfo.normal = object.transform * glm::vec4(RTUtil::GetHitNormal(object.bvhInstance.GetTriangle(hitInfo.primitiveID)), 0.0f);
	//	}
	//	else
	//	{
	//		const Primitive& hitPrim = object.primitive;

	//		switch (hitPrim.type)
	//		{
	//		case PrimitiveType_Triangle: hitInfo.normal = RTUtil::GetHitNormal(object.primitive.tri); break;
	//		case PrimitiveType_Sphere: hitInfo.normal = RTUtil::GetHitNormal(object.primitive.sphere, hitInfo.pos); break;
	//		case PrimitiveType_Plane: hitInfo.normal = RTUtil::GetHitNormal(object.primitive.plane); break;
	//		case PrimitiveType_AABB: hitInfo.normal = RTUtil::GetHitNormal(object.primitive.aabb, hitInfo.pos); break;
	//		default: FATAL_ERROR("Scene::Intersect", "Invalid primitive type"); break;
	//		}
	//	}
	//}

	return hitInfo;
}

#pragma once
#include "Core/Camera/CameraController.h"
#include "Core/Assets/AssetTypes.h"

// TODO: Can be removed at some point, scene does not need to be aware of renderer representations for meshes/objects
#include "Renderer/RaytracingTypes.h"
#include "Renderer/AccelerationStructure/BVH.h"
#include "Renderer/AccelerationStructure/TLAS.h"

struct SceneObject
{
	glm::mat4 transform;
	Material material;

	bool hasBVH = false;
	union
	{
		BVH boundingVolumeHierarchy = {};
		Primitive primitive;
	};

	SceneObject(const MeshAsset& mesh, const Material& mat,
		const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale)
	{
		transform = glm::translate(glm::identity<glm::mat4>(), pos);
		transform = transform * glm::mat4_cast(glm::quat(glm::radians(rot)));
		transform = glm::scale(transform, scale);

		BVH::BuildOptions buildOptions = {};
		boundingVolumeHierarchy.Build(mesh.vertices, mesh.indices, buildOptions);
		boundingVolumeHierarchy.SetTransform(transform);

		material = mat;
		hasBVH = true;
	}

	SceneObject(const Primitive& prim, const Material& mat)
	{
		primitive = prim;
		material = mat;
		hasBVH = false;
	}

	~SceneObject() {}

	SceneObject(const SceneObject& other)
	{
		transform = other.transform;
		material = other.material;
		hasBVH = other.hasBVH;
		if (hasBVH)
			boundingVolumeHierarchy = other.boundingVolumeHierarchy;
		else
			primitive = other.primitive;
	}
};

class Scene
{
public:
	Scene();

	void Update(float dt);
	void Render();

	void RenderUI();

	HitSurfaceData TraceRay(Ray& ray) const;

private:
	CameraController m_CameraController;

	std::vector<SceneObject> m_SceneNodes;

	TLAS m_SceneTLAS;

};

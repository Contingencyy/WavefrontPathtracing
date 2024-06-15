#pragma once
#include "Core/Camera/CameraController.h"
#include "Renderer/RaytracingTypes.h"
#include "Renderer/AccelerationStructure/BVH.h"
#include "Core/Assets/AssetTypes.h"

struct SceneObject
{
	bool hasBVH = false;
	union
	{
		BVH boundingVolumeHierarchy = {};
		Primitive primitive;
	};
	Material material;

	SceneObject(const MeshAsset& mesh, const Material& mat)
	{
		BVH::BuildOptions buildOptions = {};
		boundingVolumeHierarchy.Build(mesh.vertices, mesh.indices, buildOptions);
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
		hasBVH = other.hasBVH;
		if (hasBVH)
			boundingVolumeHierarchy = other.boundingVolumeHierarchy;
		else
			primitive = other.primitive;
		material = other.material;
	}
};

class Scene
{
public:
	Scene();

	void Update(float dt);
	void Render();

	HitSurfaceData TraceRay(Ray& ray) const;

private:
	CameraController m_CameraController;

	std::vector<SceneObject> m_SceneNodes;

};

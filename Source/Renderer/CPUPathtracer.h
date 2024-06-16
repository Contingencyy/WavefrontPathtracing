#pragma once
#include "Renderer/RendererFwd.h"

struct Camera;
struct Material;

namespace CPUPathtracer
{

	void Init(uint32_t renderWidth, uint32_t renderHeight);
	void Exit();

	void BeginFrame();
	void EndFrame();

	void BeginScene(const Camera& sceneCamera);
	void Render();
	void EndScene();
	
	void RenderUI();

	RenderMeshHandle CreateMesh(const std::span<Vertex>& vertices, const std::span<uint32_t>& indices);
	void SubmitMesh(RenderMeshHandle renderMeshHandle, const glm::mat4& transform, const Material& material);

}

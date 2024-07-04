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

	void BeginScene(const Camera& sceneCamera, RenderTextureHandle hdrEnvHandle);
	void Render();
	void EndScene();
	
	void RenderUI();

	RenderTextureHandle CreateTexture(uint32_t textureWidth, uint32_t textureHeight, const std::vector<glm::vec4>& pixelData);
	RenderMeshHandle CreateMesh(const std::span<Vertex>& vertices, const std::span<uint32_t>& indices);
	void SubmitMesh(RenderMeshHandle renderMeshHandle, const glm::mat4& transform, const Material& material);

}

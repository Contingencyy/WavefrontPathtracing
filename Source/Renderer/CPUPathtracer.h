#pragma once
#include "Renderer/RendererFwd.h"

struct Camera;
struct Material;

namespace CPUPathtracer
{

	void Init(u32 RenderWidth, u32 RenderHeight);
	void Exit();

	void BeginFrame();
	void EndFrame();

	void BeginScene(const Camera& SceneCamera, RenderTextureHandle REnvTextureHandle);
	void Render();
	void EndScene();
	
	void RenderUI();

	RenderTextureHandle CreateTexture(u32 Width, u32 Height, f32* PtrPixelData);
	RenderMeshHandle CreateMesh(Vertex* Vertices, u32 VertexCount, u32* Indices, u32 IndexCount);
	void SubmitMesh(RenderMeshHandle RMeshHandle, const glm::mat4& Transform, const Material& Mat);

}

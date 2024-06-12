#pragma once

struct Camera;
class Scene;

namespace CPUPathtracer
{

	void Init(uint32_t renderWidth, uint32_t renderHeight);
	void Exit();

	void BeginFrame();
	void EndFrame();

	void BeginScene(const Camera& sceneCamera);
	void Render(const Scene& scene);
	void EndScene();

	void RenderUI();

}

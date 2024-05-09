#pragma once

namespace CPUPathtracer
{

	void Init(uint32_t renderWidth, uint32_t renderHeight);
	void Exit();

	void BeginFrame();
	void Render();
	void EndFrame();

	void RenderUI();

}

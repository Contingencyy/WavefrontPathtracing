#pragma once

namespace DX12Backend
{

	void Init();
	void Exit();

	void BeginFrame();
	void EndFrame();

	void CopyToBackBuffer(char* pixelData);
	void Present();

}

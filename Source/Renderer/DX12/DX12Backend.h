#pragma once

namespace DX12Backend
{

	void Init();
	void Exit();

	void CopyToBackBuffer(char* pixelData, uint32_t numBytes);
	void Present();

}

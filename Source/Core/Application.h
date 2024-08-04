#pragma once

struct MemoryArena;

namespace Application
{

	void Init(MemoryArena* Arena);
	void Exit();
	void Run();

	b8 ShouldClose();

}

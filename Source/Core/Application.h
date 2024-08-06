#pragma once

struct MemoryArena;

namespace Application
{

	void Init();
	void Exit();
	void Run();

	b8 ShouldClose();

}

#pragma once

struct memory_arena_t;

namespace application
{

	void init();
	void exit();
	void run();

	b8 should_close();

}

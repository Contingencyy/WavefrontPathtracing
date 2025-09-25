#pragma once
#include "core/common.h"

struct memory_arena_t;

struct command_line_args_t
{
	int32_t window_width = 0;
	int32_t window_height = 0;
};

namespace application
{

	void init(memory_arena_t& arena, const command_line_args_t& cmd_args);
	void exit();
	void run();

	bool should_close();

}

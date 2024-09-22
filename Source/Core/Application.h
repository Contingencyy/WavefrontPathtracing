#pragma once

struct memory_arena_t;

struct command_line_args_t
{
	i32 window_width = 0;
	i32 window_height = 0;
};

namespace application
{

	void init(const command_line_args_t& cmd_args);
	void exit();
	void run();

	b8 should_close();

}

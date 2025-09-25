#include "core/memory/memory_arena.h"
#include "platform/platform.h"
#include "platform/windows/windows_common.h"
#include "core/application.h"

int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int32_t nShowCmd)
{
	memory_arena_t arena = {};
	
	command_line_args_t parsed_args = platform::parse_command_line_args(arena, lpCmdLine);
	platform::init();

	while (!application::should_close())
	{
		application::init(arena, parsed_args);
		application::run();
		application::exit();
	}

	platform::exit();

	return 0;
}

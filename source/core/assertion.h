#pragma once
#include "core/common.h"
#include "core/string/string.h"
#include "core/memory/memory_arena.h"

#include <stdarg.h>

#define ASSERT_MSG(expr, msg, ...) ((expr) ? true : (fatal_error(__LINE__, __FILE__, "Assertion", msg, ##__VA_ARGS__), false))
#define ASSERT(expr) ASSERT_MSG(expr, "Assertion failed: " #expr)

#ifdef _DEBUG
#define DEBUG_ASSERT_MSG(expr, msg, ...) ASSERT_MSG(expr, msg, ##__VA_ARGS__)
#define DEBUG_ASSERT(expr) ASSERT_MSG(expr)
#else
#define DEBUG_ASSERT_MSG(expr, msg, ...)
#define DEBUG_ASSERT(expr)
#endif

#define ALWAYS(expr) ASSERT(expr)
#define NEVER(expr) !ALWAYS(!(expr))

#define FATAL_ERROR(sender, msg, ...) fatal_error(__LINE__, __FILE__, sender, msg, ##__VA_ARGS__)

// Platform-specific implementation
void fatal_error_impl(i32 line, const string_t& error_msg);

inline void fatal_error(i32 line, const char* file, const char* sender, const char* message, ...)
{
	ARENA_SCRATCH_SCOPE()
	{
		va_list args;
		va_start(args, message);

		string_t formatted_message = ARENA_PRINTF_ARGS(arena_scratch, message, args);
		
		va_end(args);

		string_t error_message = ARENA_PRINTF(arena_scratch, "Fatal Error Occured\n[%s] %s\nFile: %s\nLine: %i\n", sender, string::make_nullterm(arena_scratch, formatted_message), file, line);
		fatal_error_impl(line, error_message);
	}
}

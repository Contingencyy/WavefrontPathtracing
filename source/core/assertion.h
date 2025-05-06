#pragma once
#include "core/common.h"
#include "core/string/string.h"
#include "core/memory/memory_arena.h"

#include <cstdarg>
#include <vadefs.h>

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
#define SHOW_MESSAGE_BOX(title, sender, msg, ...) show_message_box(title, sender, msg, ##__VA_ARGS__)

// Platform-specific implementations
void fatal_error_impl(int32_t line, const char* error_msg);
bool show_message_box_impl(const char* title, const char* message);

inline void fatal_error(int32_t line, const char* file, const char* sender, const char* message, ...)
{
	ARENA_SCRATCH_SCOPE()
	{
		va_list args;
		va_start(args, message);

		string_t formatted_message = ARENA_PRINTF_ARGS(arena_scratch, message, args);
		
		va_end(args);

		string_t error_message = ARENA_PRINTF(arena_scratch, "Fatal Error Occured\n[%s] %.*s\nFile: %s\nLine: %i\n", sender, STRING_EXPAND(formatted_message), file, line);
		const char* error_message_nullterm = string::make_nullterm(arena_scratch, error_message);
		fatal_error_impl(line, error_message_nullterm);
	}
}

inline bool show_message_box(const char* title, const char* sender, const char* message, ...)
{
	bool result = true;
	ARENA_SCRATCH_SCOPE()
	{
		va_list args;
		va_start(args, message);

		string_t formatted_message = ARENA_PRINTF_ARGS(arena_scratch, message, args);
		
		va_end(args);

		string_t full_message = ARENA_PRINTF(arena_scratch, "[%s] %.*s", sender, STRING_EXPAND(formatted_message));
		const char* message_nullterm = string::make_nullterm(arena_scratch, full_message);
		result = show_message_box_impl(title, message_nullterm);
	}

	return result;
}

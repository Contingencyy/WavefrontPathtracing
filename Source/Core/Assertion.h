#pragma once
#include "core/common.h"

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
void fatal_error_impl(i32 line, const char* error_msg);

inline void fatal_error(i32 line, const char* file, const char* sender, const char* message, ...)
{
	va_list args;
	va_start(args, message);

	// TODO: Implement custom counted string class
	char formatted_msg[1024];
	vsnprintf_s(formatted_msg, ARRAY_SIZE(formatted_msg), message, args);

	char error_msg[1024];
	sprintf_s(error_msg, ARRAY_SIZE(error_msg), "Fatal Error Occured\n[%s] %s\nFile: %s\nLine: %i\n", sender, formatted_msg, file, line);

	fatal_error_impl(line, error_msg);

	va_end(args);
}

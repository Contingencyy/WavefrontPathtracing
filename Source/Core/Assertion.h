#pragma once
#include "Core/Common.h"

#include <stdarg.h>

#define ASSERT_MSG(expr, msg, ...) ((expr) ? true : (FatalError(__LINE__, __FILE__, "Assertion", msg, ##__VA_ARGS__), false))
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

#define FATAL_ERROR(sender, msg, ...) FatalError(__LINE__, __FILE__, sender, msg, ##__VA_ARGS__)

// Platform-specific implementation
void FatalErrorImpl(i32 Line, const char* ErrorMessage);

static void FatalError(i32 Line, const char* File, const char* Sender, const char* Message, ...)
{
	va_list Args;
	va_start(Args, Message);

	// TODO: Implement custom counted string class
	char FormattedMessage[1024];
	vsnprintf_s(FormattedMessage, ARRAY_SIZE(FormattedMessage), Message, Args);

	char FullErrorMessage[1024];
	sprintf_s(FullErrorMessage, ARRAY_SIZE(FullErrorMessage), "Fatal Error Occured\n[%s] %s\nFile: %s\nLine: %i\n", Sender, FormattedMessage, File, Line);

	FatalErrorImpl(Line, FullErrorMessage);

	va_end(Args);
}

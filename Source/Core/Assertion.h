#pragma once
#include "Core/Common.h"

#include <string>
#include <format>

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
void FatalErrorImpl(i32 Line, const std::string& File, const std::string& Sender, const std::string& ErrorMessage);

template<typename... TArgs>
void FatalError(i32 Line, const std::string& File, const std::string& Sender, const std::string& Message, TArgs&&... Args)
{
	std::string FormattedMessage = std::vformat(Message, std::make_format_args(Args...));
	std::string ErrorMessage = std::format("Fatal Error Occured\n[{}] {}\nFile: {}\nLine: {}\n", Sender, FormattedMessage, File, Line);

	FatalErrorImpl(Line, File, Sender, ErrorMessage);
}

#pragma once
#include <string>
#include <format>

#define ASSERT_MSG(expr, msg, ...) FatalError(__LINE__, __FILE__, "Assert", msg, __VA_ARGS__)
#define ASSERT(expr) ASSERT_MSG(expr, "Assertion failed: " STRINGIFY(expr))

#ifdef _DEBUG
#define DEBUG_ASSERT_MSG(expr, msg, ...) ASSERT_MSG(expr, msg, __VA_ARGS__)
#define DEBUG_ASSERT(expr) ASSERT_MSG(expr)
#else
#define DEBUG_ASSERT_MSG(expr, msg, ...)
#define DEBUG_ASSERT(expr)
#endif

#define ALWAYS(expr) ASSERT(expr)
#define NEVER(expr) !ALWAYS(!(expr))

#define FATAL_ERROR(sender, msg, ...) FatalError(__LINE__, __FILE__, sender, msg, __VA_ARGS__)

void FatalErrorImpl(int line, const std::string& file, const std::string& sender, const std::string& formattedMessage);

template<typename... TArgs>
void FatalError(int line, const std::string& file, const std::string& sender, const std::string& msg, TArgs&&... args)
{
	std::string formattedMessage = std::vformat(msg, std::make_format_args(args...));
	FatalErrorImpl(line, file, sender, formattedMessage);
}

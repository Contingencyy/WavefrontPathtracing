#pragma once
#include "core/common.h"
#include "core/string/string.h"

#include <iostream>

namespace logger
{

	enum LOG_SEVERITY
	{
		LOG_SEVERITY_VERBOSE,
		LOG_SEVERITY_INFO,
		LOG_SEVERITY_WARN,
		LOG_SEVERITY_ERROR
	};

	inline constexpr LOG_SEVERITY LOG_SEVERITY_MINIMUM_LEVEL = LOG_SEVERITY_VERBOSE;

	inline constexpr const char* log_severity_to_string(LOG_SEVERITY severity)
	{
		switch (severity)
		{
		case LOG_SEVERITY_VERBOSE:	return "VERBOSE";
		case LOG_SEVERITY_INFO:		return "INFO";
		case LOG_SEVERITY_WARN:		return "WARN";
		case LOG_SEVERITY_ERROR:	return "ERROR";
		default:					return "UNKNOWN";
		}
	}

	inline void log(LOG_SEVERITY severity, const char* sender, const char* log_message, ...)
	{
		if (severity < LOG_SEVERITY_MINIMUM_LEVEL)
			return;

		ARENA_SCRATCH_SCOPE()
		{
			va_list args;
			va_start(args, log_message);

			string_t formatted_log_message = ARENA_PRINTF_ARGS(arena_scratch, log_message, args);

			va_end(args);

			string_t full_log_message = ARENA_PRINTF(arena_scratch, "[%s][%s] %s", log_severity_to_string(severity), sender, string::make_nullterm(arena_scratch, formatted_log_message));
			std::cout << string::make_nullterm(arena_scratch, full_log_message) << std::endl;
		}
	}

}

#define LOG_VERBOSE(...) logger::log(logger::LOG_SEVERITY_VERBOSE, __VA_ARGS__)
#define LOG_INFO(...)	 logger::log(logger::LOG_SEVERITY_INFO,    __VA_ARGS__)
#define LOG_WARN(...)	 logger::log(logger::LOG_SEVERITY_WARN,	  __VA_ARGS__)
#define LOG_ERR(...)	 logger::log(logger::LOG_SEVERITY_ERROR,	  __VA_ARGS__)

#pragma once
#include "core/common.h"
#include "core/string/string.h"

#include <iostream>

namespace logger
{

	enum log_severity_t
	{
		LogSeverity_Verbose,
		LogSeverity_Info,
		LogSeverity_Warn,
		LogSeverity_Error
	};

	static constexpr log_severity_t LOG_SEVERITY_MINIMUM_LEVEL = LogSeverity_Verbose;

	static constexpr const char* log_severity_to_string(log_severity_t severity)
	{
		switch (severity)
		{
		case LogSeverity_Verbose: return "VERBOSE";
		case LogSeverity_Info:    return "INFO";
		case LogSeverity_Warn:    return "WARN";
		case LogSeverity_Error:   return "ERROR";
		default:				  return "UNKNOWN";
		}
	}

	static void log(log_severity_t severity, const char* sender, const char* log_message, ...)
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

#define LOG_VERBOSE(...) logger::log(logger::LogSeverity_Verbose, __VA_ARGS__)
#define LOG_INFO(...)	 logger::log(logger::LogSeverity_Info,    __VA_ARGS__)
#define LOG_WARN(...)	 logger::log(logger::LogSeverity_Warn,	  __VA_ARGS__)
#define LOG_ERR(...)	 logger::log(logger::LogSeverity_Error,	  __VA_ARGS__)

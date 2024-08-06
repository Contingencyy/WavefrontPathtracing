#pragma once
#include <stdarg.h>
#include <iostream>

namespace Logger
{

	enum LogSeverity
	{
		LogSeverity_Verbose,
		LogSeverity_Info,
		LogSeverity_Warn,
		LogSeverity_Error
	};

	static constexpr LogSeverity LOG_SEVERITY_MINIMUM_LEVEL = LogSeverity_Verbose;

	static constexpr const char* LogSeverityToString(LogSeverity Severity)
	{
		switch (Severity)
		{
		case LogSeverity_Verbose: return "VERBOSE";
		case LogSeverity_Info:    return "INFO";
		case LogSeverity_Warn:    return "WARN";
		case LogSeverity_Error:   return "ERROR";
		default:				  return "UNKNOWN";
		}
	}

	static void Log(LogSeverity Severity, const char* Sender, const char* LogMessage, ...)
	{
		if (Severity < LOG_SEVERITY_MINIMUM_LEVEL)
			return;

		va_list Args;
		va_start(Args, LogMessage);

		// TODO: Implement custom counted string class
		char LogMessageFormatted[512];
		vsnprintf_s(LogMessageFormatted, ARRAY_SIZE(LogMessageFormatted), LogMessage, Args);

		char FinalMessage[512];
		sprintf_s(FinalMessage, ARRAY_SIZE(FinalMessage), "[%s] [%s]\t%s", LogSeverityToString(Severity), Sender, LogMessageFormatted);

		va_end(Args);

		std::cout << FinalMessage << std::endl;
	}

}

#define LOG_VERBOSE(...) Logger::Log(Logger::LogSeverity_Verbose, __VA_ARGS__)
#define LOG_INFO(...)	 Logger::Log(Logger::LogSeverity_Info,    __VA_ARGS__)
#define LOG_WARN(...)	 Logger::Log(Logger::LogSeverity_Warn,	  __VA_ARGS__)
#define LOG_ERR(...)	 Logger::Log(Logger::LogSeverity_Error,	  __VA_ARGS__)

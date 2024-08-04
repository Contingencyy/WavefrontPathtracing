#pragma once

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

	template<typename... TArgs>
	void Log(LogSeverity Severity, const std::string& Sender, const std::string& LogMessage, TArgs&&... Args)
	{
		/*if (Severity < LOG_SEVERITY_MINIMUM_LEVEL)
			return;

		std::string FormattedLogMessage = std::format("[{}]\t[{}]\t", LogSeverityToString(Severity), Sender) +
			std::vformat(LogMessage, std::make_format_args(Args...));

		std::cout << FormattedLogMessage << std::endl;*/
	}

}

#define LOG_VERBOSE(...) Logger::Log(Logger::LogSeverity_Verbose, __VA_ARGS__)
#define LOG_INFO(...)	 Logger::Log(Logger::LogSeverity_Info,    __VA_ARGS__)
#define LOG_WARN(...)	 Logger::Log(Logger::LogSeverity_Warn,	  __VA_ARGS__)
#define LOG_ERR(...)	 Logger::Log(Logger::LogSeverity_Error,	  __VA_ARGS__)

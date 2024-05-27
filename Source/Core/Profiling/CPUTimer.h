#pragma once
#include "Profiler.h"

#include <chrono>

class CPUTimer
{

public:
	CPUTimer(GlobalProfilingScope scope);
	~CPUTimer();

	void End();

private:
	GlobalProfilingScope m_ProfilingScope;
	bool m_Ended = false;

	std::chrono::time_point<std::chrono::steady_clock> m_TimePointStart;
	std::chrono::time_point<std::chrono::steady_clock> m_TimePointEnd;

};

#define PROFILE_SCOPE(scope) CPUTimer cpuTimer##scope(scope)
#define PROFILE_BEGIN(scope) CPUTimer cpuTimer##scope(scope)
#define PROFILE_END(scope) cpuTimer##scope.End()

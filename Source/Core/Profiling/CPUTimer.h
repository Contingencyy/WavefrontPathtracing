#pragma once
#include "Profiler.h"

#include <chrono>

class CPUTimer
{

public:
	CPUTimer() = default;
	CPUTimer(GlobalProfilingScope scope);
	~CPUTimer();

	void End();
	float ElapsedInSeconds() const;

private:
	GlobalProfilingScope m_ProfilingScope = GlobalProfilingScope_NumScopes;
	bool m_Ended = false;

	std::chrono::time_point<std::chrono::steady_clock> m_TimePointStart;
	std::chrono::time_point<std::chrono::steady_clock> m_TimePointEnd;
	float m_ElapsedSeconds = 0.0f;

};

#define PROFILE_SCOPE(scope) CPUTimer cpuTimer##scope(scope)
#define PROFILE_BEGIN(scope) CPUTimer cpuTimer##scope(scope)
#define PROFILE_END(scope) cpuTimer##scope.End()

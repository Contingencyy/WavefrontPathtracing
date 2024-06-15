#include "Pch.h"
#include "CPUTimer.h"
#include "Profiler.h"

CPUTimer::CPUTimer(GlobalProfilingScope scope)
	: m_ProfilingScope(scope)
{
	m_TimePointStart = std::chrono::steady_clock::now();
}

CPUTimer::~CPUTimer()
{
	End();
}

void CPUTimer::End()
{
	if (!m_Ended)
	{
		m_Ended = true;

		m_TimePointEnd = std::chrono::steady_clock::now();
		std::chrono::duration<float> elapsed = m_TimePointEnd - m_TimePointStart;
		m_ElapsedSeconds = elapsed.count();

		if (m_ProfilingScope != GlobalProfilingScope_NumScopes)
		{
			Profiler::AddTimerResult(m_ProfilingScope, elapsed.count());
		}
	}
}

float CPUTimer::ElapsedInSeconds() const
{
	return m_ElapsedSeconds;
}

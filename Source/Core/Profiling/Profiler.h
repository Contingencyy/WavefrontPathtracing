#pragma once

enum GlobalProfilingScope
{
	GlobalProfilingScope_RenderTime,
	GlobalProfilingScope_NumScopes
};

namespace Profiler
{

	struct TimerResult
	{
		float lastElapsed = 0.0f;
	};

	void AddTimerResult(GlobalProfilingScope scope, float timeElapsed);
	TimerResult GetTimerResult(GlobalProfilingScope scope);

}

#include "Pch.h"
#include "Profiler.h"

namespace Profiler
{

	struct Instance
	{
		std::unordered_map<GlobalProfilingScope, TimerResult> timerResults;
	} static inst;

	void AddTimerResult(GlobalProfilingScope scope, float timeElapsed)
	{
		if (inst.timerResults.find(scope) == inst.timerResults.end())
			inst.timerResults.emplace(scope, TimerResult());

		inst.timerResults.at(scope).lastElapsed = timeElapsed;
	}

	TimerResult GetTimerResult(GlobalProfilingScope scope)
	{
		if (inst.timerResults.find(scope) != inst.timerResults.end())
			return inst.timerResults.at(scope);

		return {};
	}

}

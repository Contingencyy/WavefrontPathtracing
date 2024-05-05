#include "Pch.h"
#include "Application.h"
#include "Logger.h"

bool PollWindowEvents();

namespace Application
{

	static bool s_should_close = false;

	struct Instance
	{
		bool is_running = false;
	} static *inst;

	void Init()
	{
		inst = new Instance;
		inst->is_running = true;

		LOG_INFO("Application", "Init");
	}

	void Exit()
	{
		delete inst;

		LOG_INFO("Application", "Exit");
	}

	void Run()
	{
		while (inst->is_running && !s_should_close)
		{
			LOG_INFO("Application", "Run");
			s_should_close = !PollWindowEvents();
		}
	}

	bool ShouldClose()
	{
		return s_should_close;
	}

}

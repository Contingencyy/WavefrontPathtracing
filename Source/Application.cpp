#include "Pch.h"
#include "Application.h"
#include "Logger.h"

#include "Renderer/DX12/DX12Backend.h"

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

		DX12Backend::Init();

		inst->is_running = true;
		LOG_INFO("Application", "Initialized");
	}

	void Exit()
	{
		DX12Backend::Exit();

		delete inst;

		LOG_INFO("Application", "Exited");
	}

	void Run()
	{
		while (inst->is_running && !s_should_close)
		{
			s_should_close = !PollWindowEvents();
		}
	}

	bool ShouldClose()
	{
		return s_should_close;
	}

}

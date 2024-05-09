#include "Pch.h"
#include "Application.h"
#include "Logger.h"
#include "Renderer/CPUPathtracer.h"

bool PollWindowEvents();
void GetWindowSize(uint32_t& windowWidth, uint32_t& windowHeight);

namespace Application
{

	static bool s_should_close = false;

	struct Instance
	{
		bool is_running = false;
	} static *inst;

	void Init()
	{
		LOG_INFO("Application", "Init");
		
		inst = new Instance();

		uint32_t windowWidth = 0, windowHeight = 0;
		GetWindowSize(windowWidth, windowHeight);

		CPUPathtracer::Init(windowWidth, windowHeight);

		inst->is_running = true;
	}

	void Exit()
	{
		LOG_INFO("Application", "Exit");

		CPUPathtracer::Exit();

		delete inst;
	}

	void Run()
	{
		while (inst->is_running && !s_should_close)
		{
			s_should_close = !PollWindowEvents();

			CPUPathtracer::BeginFrame();
			CPUPathtracer::Render();
			CPUPathtracer::EndFrame();
		}
	}

	bool ShouldClose()
	{
		return s_should_close;
	}

}

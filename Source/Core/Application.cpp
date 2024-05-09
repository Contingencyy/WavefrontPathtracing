#include "Pch.h"
#include "Application.h"
#include "Logger.h"
#include "Renderer/CPUPathtracer.h"

#include "imgui/imgui.h"

#include <chrono>

bool PollWindowEvents();
void GetWindowSize(uint32_t& windowWidth, uint32_t& windowHeight);

namespace Application
{

	static bool s_should_close = false;

	struct Instance
	{
		std::chrono::duration<float> deltaTime = std::chrono::duration<float>(0.0f);
		bool is_running = false;
	} static *inst;

	static void RenderUI()
	{
		ImGui::Begin("General");

		float frametimeInMs = inst->deltaTime.count() * 1000.0f;
		uint32_t fps = 1000.0f / frametimeInMs;

		ImGui::Text("FPS: %u", fps);
		ImGui::Text("Frametime: %.3f ms", frametimeInMs);

		ImGui::End();

		CPUPathtracer::RenderUI();
	}

	static void Render()
	{
		CPUPathtracer::BeginFrame();
		CPUPathtracer::Render();
		RenderUI();
		CPUPathtracer::EndFrame();
	}

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
		std::chrono::high_resolution_clock::time_point timeCurr = std::chrono::high_resolution_clock::now();
		std::chrono::high_resolution_clock::time_point timePrev = std::chrono::high_resolution_clock::now();

		while (inst->is_running && !s_should_close)
		{
			timeCurr = std::chrono::high_resolution_clock::now();
			inst->deltaTime = timeCurr - timePrev;

			s_should_close = !PollWindowEvents();
			Render();

			timePrev = timeCurr;
		}
	}

	bool ShouldClose()
	{
		return s_should_close;
	}

}

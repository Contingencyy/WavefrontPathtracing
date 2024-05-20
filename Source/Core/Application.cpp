#include "Pch.h"
#include "Application.h"
#include "Core/Logger.h"
#include "Core/Scene.h"
#include "Core/Input.h"
#include "Renderer/CPUPathtracer.h"

#include "imgui/imgui.h"

#include <chrono>

void GetWindowClientArea(int32_t& windowWidth, int32_t& windowHeight);
void SetWindowCaptureMouse(bool capture);
bool PollWindowEvents();

namespace Application
{

	static bool s_should_close = false;

	struct Instance
	{
		Scene activeScene;

		std::chrono::duration<float> deltaTime = std::chrono::duration<float>(0.0f);
		bool is_running = false;
	} static *inst;

	static void HandleEvents()
	{
		s_should_close = !PollWindowEvents();

		if (!ImGui::GetIO().WantCaptureMouse)
		{
			if (!Input::IsMouseCaptured() && Input::IsKeyPressed(Input::KeyCode_LeftMouse))
			{
				SetWindowCaptureMouse(true);
			}
		}
		if (Input::IsMouseCaptured())
		{
			if (Input::IsKeyPressed(Input::KeyCode_RightMouse))
				SetWindowCaptureMouse(false);
		}
	}

	static void Update()
	{
		inst->activeScene.Update(inst->deltaTime.count());
	}

	static void RenderUI()
	{
		ImGui::Begin("General");

		uint32_t fps = 1.0f / inst->deltaTime.count();
		float frametimeInMs = inst->deltaTime.count() * 1000.0f;

		ImGui::Text("FPS: %u", fps);
		ImGui::Text("Frametime: %.3f ms", frametimeInMs);

		ImGui::End();

		CPUPathtracer::RenderUI();
	}

	static void Render()
	{
		CPUPathtracer::BeginFrame();
		inst->activeScene.Render();
		RenderUI();
		CPUPathtracer::EndFrame();
	}

	void Init()
	{
		LOG_INFO("Application", "Init");
		
		inst = new Instance();

		int32_t clientWidth = 0, clientHeight = 0;
		GetWindowClientArea(clientWidth, clientHeight);

		CPUPathtracer::Init(clientWidth, clientHeight);

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

			HandleEvents();
			Update();
			Render();

			timePrev = timeCurr;
		}
	}

	bool ShouldClose()
	{
		return s_should_close;
	}

}

#include "Pch.h"
#include "Application.h"
#include "Core/Logger.h"
#include "Core/Scene.h"
#include "Core/Input.h"
#include "Renderer/CPUPathtracer.h"

#include "imgui/imgui.h"

#include <chrono>

void GetWindowClientArea(i32& WindowWidth, i32& WindowHeight);
void SetWindowCaptureMouse(b8 bCapture);
b8 PollWindowEvents();

namespace Application
{

	static b8 s_ShouldClose = false;

	struct Instance
	{
		MemoryArena* Arena;

		Scene* ActiveScene;

		std::chrono::duration<f32> DeltaTime = std::chrono::duration<f32>(0.0f);
		b8 bRunning = false;
	} static *Inst;

	static void HandleEvents()
	{
		s_ShouldClose = !PollWindowEvents();

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
			{
				SetWindowCaptureMouse(false);
			}
		}
	}

	static void Update()
	{
		Inst->ActiveScene->Update(Inst->DeltaTime.count());
	}

	static void RenderUI()
	{
		ImGui::Begin("General");

		u32 FPS = 1.0f / Inst->DeltaTime.count();
		f32 FrameTimeMS = Inst->DeltaTime.count() * 1000.0f;

		ImGui::Text("FPS: %u", FPS);
		ImGui::Text("Frametime: %.3f ms", FrameTimeMS);

		ImGui::End();

		Inst->ActiveScene->RenderUI();
		CPUPathtracer::RenderUI();
	}

	static void Render()
	{
		CPUPathtracer::BeginFrame();
		Inst->ActiveScene->Render();
		RenderUI();
		CPUPathtracer::EndFrame();
	}

	void Init(MemoryArena* Arena)
	{
		LOG_INFO("Application", "Init");

		Inst = ARENA_ALLOC_STRUCT_ZERO(Arena, Instance);
		Inst->Arena = Arena;

		i32 ClientWidth = 0, ClientHeight = 0;
		GetWindowClientArea(ClientWidth, ClientHeight);

		// We could do one arena per system eventually, but for now everything that needs
		// to live for as long as the application does will use the same arena
		CPUPathtracer::Init(Inst->Arena, ClientWidth, ClientHeight);

		Inst->ActiveScene = ARENA_ALLOC_STRUCT_ZERO(Arena, Scene);
		Inst->ActiveScene->Init();

		Inst->bRunning = true;
	}

	void Exit()
	{
		LOG_INFO("Application", "Exit");

		CPUPathtracer::Exit();

		Inst->ActiveScene->Destroy();
		ARENA_RELEASE(Inst->Arena);
	}

	void Run()
	{
		std::chrono::high_resolution_clock::time_point TimeCurr = std::chrono::high_resolution_clock::now();
		std::chrono::high_resolution_clock::time_point TimePrev = std::chrono::high_resolution_clock::now();

		while (Inst->bRunning && !s_ShouldClose)
		{
			TimeCurr = std::chrono::high_resolution_clock::now();
			Inst->DeltaTime = TimeCurr - TimePrev;

			HandleEvents();
			Update();
			Render();

			TimePrev = TimeCurr;
		}
	}

	b8 ShouldClose()
	{
		return s_ShouldClose;
	}

}

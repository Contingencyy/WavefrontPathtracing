#include "Pch.h"
#include "CPUPathtracer.h"
#include "RaytracingUtils.h"
#include "Renderer/Camera.h"
#include "DX12/DX12Backend.h"
#include "Core/Logger.h"
#include "Core/Scene.h"

namespace CPUPathtracer
{

	struct Instance
	{
		Camera sceneCamera;

		uint32_t renderWidth = 0;
		uint32_t renderHeight = 0;
		float invRenderWidth = 0.0f;
		float invRenderHeight = 0.0f;

		std::vector<uint32_t> pixelBuffer;

		uint64_t frameIndex = 0;
	} static *inst;

	void Init(uint32_t renderWidth, uint32_t renderHeight)
	{
		LOG_INFO("CPUPathtracer", "Init");

		inst = new Instance();
		inst->sceneCamera = Camera(glm::vec3(), glm::vec3(0.0f, 0.0f, 1.0f), 60.0f);

		inst->renderWidth = renderWidth;
		inst->renderHeight = renderHeight;
		inst->invRenderWidth = 1.0f / static_cast<float>(inst->renderWidth);
		inst->invRenderHeight = 1.0f / static_cast<float>(inst->renderHeight);

		inst->pixelBuffer.resize(inst->renderWidth * inst->renderHeight);

		DX12Backend::Init();
	}

	void Exit()
	{
		LOG_INFO("CPUPathtracer", "Exited");
		
		DX12Backend::Exit();

		delete inst;
	}

	void BeginFrame()
	{
		DX12Backend::BeginFrame();
	}

	void Render()
	{
		float aspectRatio = inst->renderWidth / static_cast<float>(inst->renderHeight);

		for (uint32_t y = 0; y < inst->renderHeight; ++y)
		{
			for (uint32_t x = 0; x < inst->renderWidth; ++x)
			{
				Ray ray = RTUtil::ConstructCameraRay(inst->sceneCamera, x, y,
					aspectRatio, inst->invRenderWidth, inst->invRenderHeight);

				Sphere sphere = { .center = glm::vec3(0.0f, 0.0f, 10.0f), .radiusSquared = 16.0f };
				bool hit = RTUtil::Intersect(sphere, ray);

				glm::vec4 finalColor = {};
				if (hit)
					finalColor = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
				else
					finalColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

				uint32_t pixelPos = y * inst->renderWidth + x;
				inst->pixelBuffer[pixelPos] = RTUtil::Vec4ToUint32(finalColor);
			}
		}
	}

	void EndFrame()
	{
		DX12Backend::EndFrame();
		DX12Backend::CopyToBackBuffer(reinterpret_cast<char*>(inst->pixelBuffer.data()), inst->pixelBuffer.size() * 4);
		DX12Backend::Present();

		inst->frameIndex++;
	}

}

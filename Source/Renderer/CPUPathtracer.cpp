#include "Pch.h"
#include "CPUPathtracer.h"
#include "RaytracingUtils.h"
#include "Renderer/Camera.h"
#include "Renderer/CameraController.h"
#include "DX12/DX12Backend.h"
#include "Core/Logger.h"
#include "Core/Random.h"
#include "Core/Threadpool.h"
#include "Core/Profiling/Profiler.h"
#include "Core/Profiling/CPUTimer.h"

#include "imgui/imgui.h"

#include "Core/Scene.h"

namespace CPUPathtracer
{

	struct RenderSettings
	{
		glm::vec3 skyColor = glm::vec3(0.52f, 0.8f, 0.92f);
		float skyColorIntensity = 0.5f;

		bool linearToSRGB = true;
	};

	struct Instance
	{
		Camera sceneCamera;

		uint32_t renderWidth = 0;
		uint32_t renderHeight = 0;
		float invRenderWidth = 0.0f;
		float invRenderHeight = 0.0f;

		std::vector<uint32_t> pixelBuffer;
		std::vector<glm::vec4> pixelAccumulator;
		uint32_t numAccumulatedFrames = 0;

		Threadpool renderThreadpool;

		uint64_t frameIndex = 0;

		RenderSettings settings;
	} static *inst;

	static void ResetAccumulation()
	{
		// Clear the accumulator, reset accumulated frame count
		std::fill(inst->pixelAccumulator.begin(), inst->pixelAccumulator.end(), glm::vec4(0.0f));
		inst->numAccumulatedFrames = 0;
	}

	static glm::vec4 TracePath(Scene* scene, Ray& ray)
	{
		// TODO: Proper materials for objects
		// TODO: Multithreading
		// TODO: Spawn new objects from ImGui
		// TODO: Ray depth debug view
		// TODO: Ray/path visualization mode
		// FIX/TEST: Shadow on sphere is not correct, it should interreflect the plane, then realize that the part of the plane lies in shadow, so it should be black

		glm::vec3 throughput(1.0f);
		glm::vec3 energy(0.0f);

		bool isSpecularRay = false;
		uint32_t currRayDepth = 0;

		while (currRayDepth <= RAY_MAX_DEPTH)
		{
			HitSurfaceData hit = scene->Intersect(ray);

			if (hit.objIdx == ~0u)
			{
				energy += inst->settings.skyColor * inst->settings.skyColorIntensity * throughput;
				break;
			}

			Material hitMat = hit.objMat;

			// If the surface is emissive, we simply add its color to the energy output and stop tracing
			if (hitMat.isEmissive)
			{
				energy += hitMat.emissive * hitMat.intensity * throughput;
				break;
			}

			float r = Random::Float();

			if (r < hitMat.specular)
			{
				glm::vec3 specularDir = RTUtil::Reflect(ray.dir, hit.normal);
				ray = Ray(hit.pos + specularDir * RAY_NUDGE_MODIFIER, specularDir);

				throughput *= hitMat.albedo;
				isSpecularRay = true;
			}
			else if (r < hitMat.specular + hitMat.refractivity)
			{

			}
			else
			{
				glm::vec3 diffuseBRDF = hitMat.albedo * INV_PI;

				glm::vec3 diffuseDir = RTUtil::UniformHemisphereSample(hit.normal);
				float NdotR = glm::dot(diffuseDir, hit.normal);
				float hemispherePDF = INV_TWO_PI;

				ray = Ray(hit.pos + diffuseDir * RAY_NUDGE_MODIFIER, diffuseDir);
				throughput *= (NdotR * diffuseBRDF) / hemispherePDF;
				isSpecularRay = false;
			}

			currRayDepth++;
		}

		return glm::vec4(energy, 1.0f);
	}

	void Init(uint32_t renderWidth, uint32_t renderHeight)
	{
		LOG_INFO("CPUPathtracer", "Init");

		inst = new Instance();

		inst->renderWidth = renderWidth;
		inst->renderHeight = renderHeight;
		inst->invRenderWidth = 1.0f / static_cast<float>(inst->renderWidth);
		inst->invRenderHeight = 1.0f / static_cast<float>(inst->renderHeight);

		inst->pixelBuffer.resize(inst->renderWidth * inst->renderHeight);
		inst->pixelAccumulator.resize(inst->renderWidth * inst->renderHeight);

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

	void EndFrame()
	{
		DX12Backend::EndFrame();
		DX12Backend::CopyToBackBuffer(reinterpret_cast<char*>(inst->pixelBuffer.data()));
		DX12Backend::Present();

		inst->frameIndex++;
	}

	void BeginScene(const Camera& sceneCamera)
	{
		if (inst->sceneCamera.viewMatrix != sceneCamera.viewMatrix)
			ResetAccumulation();

		inst->sceneCamera = sceneCamera;
	}

	void Render(Scene* scene)
	{
		PROFILE_SCOPE(GlobalProfilingScope_RenderTime);
		inst->numAccumulatedFrames++;

		float aspectRatio = inst->renderWidth / static_cast<float>(inst->renderHeight);
		float tanFOV = glm::tan(glm::radians(inst->sceneCamera.vfov) / 2.0f);

#if 0
		for (uint32_t y = 0; y < inst->renderHeight; ++y)
		{
			for (uint32_t x = 0; x < inst->renderWidth; ++x)
			{
				// Construct primary ray from camera through the current pixel
				Ray ray = RTUtil::ConstructCameraRay(inst->sceneCamera, x, y, tanFOV,
					aspectRatio, inst->invRenderWidth, inst->invRenderHeight);

				// Start tracing path through pixel
				glm::vec4 tracedColor = TracePath(scene, ray);

				// Update accumulator
				uint32_t pixelPos = y * inst->renderWidth + x;
				inst->pixelAccumulator[pixelPos] += tracedColor;

				// Determine final color for pixel
				glm::vec4 finalColor = inst->pixelAccumulator[pixelPos] /
					static_cast<float>(inst->numAccumulatedFrames);
				finalColor.xyz = RTUtil::TonemapReinhard(finalColor.xyz);
				if (inst->settings.linearToSRGB)
					finalColor.xyz = RTUtil::LinearToSRGB(finalColor.xyz);

				inst->pixelBuffer[pixelPos] = RTUtil::Vec4ToUint32(finalColor);
			}
		}
#else
		glm::uvec2 dispatchDim = { 16, 16 };
		ASSERT(inst->renderWidth % dispatchDim.x == 0);
		ASSERT(inst->renderHeight % dispatchDim.y == 0);

		auto tracePathThroughPixelJob = [&dispatchDim, &aspectRatio, &tanFOV, &scene](Threadpool::JobDispatchArgs dispatchArgs) {
			const uint32_t dispatchX = (dispatchArgs.jobIndex * dispatchDim.x) % inst->renderWidth;
			const uint32_t dispatchY = ((dispatchArgs.jobIndex * dispatchDim.x) / inst->renderWidth) * dispatchDim.y;

			for (uint32_t y = dispatchY; y < dispatchY + dispatchDim.y; ++y)
			{
				for (uint32_t x = dispatchX; x < dispatchX + dispatchDim.x; ++x)
				{
					// Construct primary ray from camera through the current pixel
					Ray ray = RTUtil::ConstructCameraRay(inst->sceneCamera, x, y, tanFOV,
						aspectRatio, inst->invRenderWidth, inst->invRenderHeight);

					// Start tracing path through pixel
					glm::vec4 tracedColor = TracePath(scene, ray);

					// Update accumulator
					uint32_t pixelPos = y * inst->renderWidth + x;
					inst->pixelAccumulator[pixelPos] += tracedColor;

					// Determine final color for pixel
					glm::vec4 finalColor = inst->pixelAccumulator[pixelPos] /
						static_cast<float>(inst->numAccumulatedFrames);
					finalColor.xyz = RTUtil::TonemapReinhard(finalColor.xyz);
					if (inst->settings.linearToSRGB)
						finalColor.xyz = RTUtil::LinearToSRGB(finalColor.xyz);

					inst->pixelBuffer[pixelPos] = RTUtil::Vec4ToUint32(finalColor);
				}
			}
		};

		uint32_t numPixels = inst->renderWidth * inst->renderHeight;
		uint32_t numJobs = numPixels / (dispatchDim.x * dispatchDim.y);

		inst->renderThreadpool.Dispatch(numJobs, 16, tracePathThroughPixelJob);
		inst->renderThreadpool.WaitAll();
#endif
	}

	void EndScene()
	{
	}

	void RenderUI()
	{
		if (ImGui::Begin("Renderer"))
		{
			ImGui::Text("Render time: %.3f ms", Profiler::GetTimerResult(GlobalProfilingScope_RenderTime).lastElapsed * 1000.0f);

			ImGui::Text("Resolution: %ux%u", inst->renderWidth, inst->renderHeight);
			ImGui::Text("Accumulated frames: %u", inst->numAccumulatedFrames);

			ImGui::ColorEdit3("Sky color", &inst->settings.skyColor[0], ImGuiColorEditFlags_DisplayRGB);
			ImGui::DragFloat("Sky color intensity", &inst->settings.skyColorIntensity, 0.001f);
			ImGui::Checkbox("Linear to SRGB", &inst->settings.linearToSRGB);
		}
		ImGui::End();
	}

}

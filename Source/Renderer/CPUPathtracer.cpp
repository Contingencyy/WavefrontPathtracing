#include "Pch.h"
#include "CPUPathtracer.h"
#include "RaytracingUtils.h"
#include "Renderer/Renderer.h"
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

using namespace Renderer;

namespace CPUPathtracer
{

	struct RenderSettings
	{
		RenderDataVisualization renderDataVisualization = RenderDataVisualization_None;

		bool cosineWeightedDiffuseReflection = true;
		bool russianRoulette = true;

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
		// TODO: Next event estimation
		// TODO: Total energy received, accumulated over time so it slowly stabilizes, for comparisons
		// TODO: Crash when command line is missing --width and --height
		// TODO: Spawn new objects from ImGui
		// TODO: Scene hierarchy
		// TODO: ImGuizmo to transform objects
		// TODO: Ray/path visualization mode
		// TODO: Unit tests for RTUtil functions like UniformHemisphereSampling (testing the resulting dir for length 1 for example)

		glm::vec3 throughput(1.0f);
		glm::vec3 energy(0.0f);

		bool isSpecularRay = false;
		uint32_t currRayDepth = 0;
		bool survivedRR = true;

		while (currRayDepth <= RAY_MAX_RECURSION_DEPTH)
		{
			HitSurfaceData hit = scene->Intersect(ray);

			// Handle any render data visualization that can happen after a single trace
			if (inst->settings.renderDataVisualization != RenderDataVisualization_None)
			{
				bool stopTracingPath = false;

				switch (inst->settings.renderDataVisualization)
				{
				case RenderDataVisualization_HitAlbedo:		 energy = hit.objMat.albedo; stopTracingPath = true; break;
				case RenderDataVisualization_HitNormal:		 energy = glm::abs(hit.normal); stopTracingPath = true; break;
				case RenderDataVisualization_HitSpecRefract: energy = glm::vec3(hit.objMat.specular, hit.objMat.refractivity, 0.0f); stopTracingPath = true; break;
				case RenderDataVisualization_HitAbsorption:  energy = glm::vec3(hit.objMat.absorption); stopTracingPath = true; break;
				case RenderDataVisualization_HitEmissive:	 energy = glm::vec3(hit.objMat.emissive * hit.objMat.intensity * static_cast<float>(hit.objMat.isEmissive)); stopTracingPath = true; break;
				case RenderDataVisualization_Depth:			 energy = glm::vec3(ray.t); stopTracingPath = true; break;
				}

				if (stopTracingPath)
					break;
			}

			// Add sky color to energy if we have not hit an object, and terminate path
			if (hit.objIdx == ~0u)
			{
				energy += inst->settings.skyColor * inst->settings.skyColorIntensity * throughput;
				break;
			}

			Material& hitMat = hit.objMat;

			// If the surface is emissive, we simply add its color to the energy output and stop tracing
			if (hitMat.isEmissive)
			{
				energy += hitMat.emissive * hitMat.intensity * throughput;
				break;
			}

			// Russian roulette - Stop tracing the path with a probability that is based on the material albedo
			// Since the throughput is gonna be dependent on the albedo, dark albedos carry less energy to the eye,
			// so we can eliminate those paths with a higher probability and safe ourselves the time continuing tracing low impact paths.
			// Need to adjust the throughput if the path survives since that path should carry a higher weight based on its survival probability.
			if (inst->settings.russianRoulette)
			{
				float survivalProbability = RTUtil::SurvivalProbabilityRR(hitMat.albedo);
				if (survivalProbability < Random::Float())
				{
					survivedRR = false;
					break;
				}
				else
				{
					throughput *= 1.0f / survivalProbability;
				}
			}

			// Get a random 0..1 range float to determine which path to take
			float r = Random::Float();

			// Specular path
			if (r < hitMat.specular)
			{
				glm::vec3 specularDir = RTUtil::Reflect(ray.dir, hit.normal);
				ray = Ray(hit.pos + specularDir * RAY_NUDGE_MODIFIER, specularDir);

				throughput *= hitMat.albedo;
				isSpecularRay = true;
			}
			// Refraction path
			else if (r < hitMat.specular + hitMat.refractivity)
			{
				glm::vec3 N = hit.normal;

				float cosi = glm::clamp(glm::dot(N, ray.dir), -1.0f, 1.0f);
				float etai = 1.0f, etat = hitMat.ior;

				float Fr = 1.0f;
				bool inside = true;

				// Figure out if we are inside or outside of the object we've hit
				if (cosi < 0.0f)
				{
					cosi = -cosi;
					inside = false;
				}
				else
				{
					std::swap(etai, etat);
					N = -N;
				}

				float eta = etai / etat;
				float k = 1.0f - eta * eta * (1.0f - cosi * cosi);

				// Follow refraction or specular path
				if (k >= 0.0f)
				{
					glm::vec3 refractDir = RTUtil::Refract(ray.dir, N, eta, cosi, k);

					float cosIn = glm::dot(ray.dir, hit.normal);
					float cosOut = glm::dot(refractDir, hit.normal);

					Fr = RTUtil::Fresnel(cosIn, cosOut, etai, etat);

					if (Random::Float() > Fr)
					{
						throughput *= hitMat.albedo;

						if (inside)
						{
							glm::vec3 absorption = glm::vec3(
								glm::exp(-hitMat.absorption.x * ray.t),
								glm::exp(-hitMat.absorption.y * ray.t),
								glm::exp(-hitMat.absorption.z * ray.t)
							);
							throughput *= absorption;
						}

						ray = Ray(hit.pos + refractDir * RAY_NUDGE_MODIFIER, refractDir);
						isSpecularRay = true;
					}
					else
					{
						glm::vec3 specularDir = RTUtil::Reflect(ray.dir, hit.normal);
						ray = Ray(hit.pos + specularDir * RAY_NUDGE_MODIFIER, specularDir);

						throughput *= hitMat.albedo;
						isSpecularRay = true;
					}
				}
			}
			// Diffuse path
			else
			{
				glm::vec3 diffuseBRDF = hitMat.albedo * INV_PI;
				glm::vec3 diffuseDir(0.0f);

				float NdotR = 0.0f;
				float hemispherePDF = 0.0f;

				// Cosine-weighted diffuse reflection
				if (inst->settings.cosineWeightedDiffuseReflection)
				{
					diffuseDir = RTUtil::CosineWeightedDiffuseReflection(hit.normal);
					NdotR = glm::dot(diffuseDir, hit.normal);
					hemispherePDF = NdotR / PI;
				}
				// Uniform hemisphere diffuse reflection
				else
				{
					diffuseDir = RTUtil::UniformHemisphereSample(hit.normal);
					NdotR = glm::dot(diffuseDir, hit.normal);
					hemispherePDF = INV_TWO_PI;
				}

				ray = Ray(hit.pos + diffuseDir * RAY_NUDGE_MODIFIER, diffuseDir);
				throughput *= (NdotR * diffuseBRDF) / hemispherePDF;
				isSpecularRay = false;
			}

			currRayDepth++;
		}

		// Handle any render data visualization that needs to trace the full path first
		if (inst->settings.renderDataVisualization != RenderDataVisualization_None)
		{
			switch (inst->settings.renderDataVisualization)
			{
			case RenderDataVisualization_RayRecursionDepth:
			{
				energy = glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), currRayDepth / static_cast<float>(RAY_MAX_RECURSION_DEPTH));
			} break;
			case RenderDataVisualization_RussianRouletteKillDepth:
			{
				float weight = glm::clamp((currRayDepth / static_cast<float>(RAY_MAX_RECURSION_DEPTH)) + static_cast<float>(survivedRR), 0.0f, 1.0f);
				energy = glm::mix(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), weight);
			} break;
			}
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

		if (inst->settings.renderDataVisualization != RenderDataVisualization_None)
			ResetAccumulation();
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

					// Apply simple tonemapper
					finalColor.xyz = RTUtil::TonemapReinhard(finalColor.xyz);

					// Convert final color from linear to SRGB, if enabled, and if we do not use any render data visualization
					if (inst->settings.linearToSRGB &&
						(inst->settings.renderDataVisualization == RenderDataVisualization_None ||
						inst->settings.renderDataVisualization == RenderDataVisualization_HitEmissive ||
						inst->settings.renderDataVisualization == RenderDataVisualization_HitAbsorption))
						finalColor.xyz = RTUtil::LinearToSRGB(finalColor.xyz);

					// Write final color
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

			ImGui::Text("Max Ray Recursion Depth: %u", RAY_MAX_RECURSION_DEPTH);

			// Debug category
			if (ImGui::CollapsingHeader("Debug"))
			{
				// Render data visualization
				ImGui::Text("Render data visualization mode");
				if (ImGui::BeginCombo("##Render data visualization mode", RenderDataVisualizationLabels[inst->settings.renderDataVisualization].c_str(), ImGuiComboFlags_None))
				{
					for (uint32_t i = 0; i < RenderDataVisualization_Count; ++i)
					{
						bool is_selected = i == inst->settings.renderDataVisualization;

						if (ImGui::Selectable(RenderDataVisualizationLabels[i].c_str(), is_selected))
						{
							inst->settings.renderDataVisualization = (RenderDataVisualization)i;
							ResetAccumulation();
						}

						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}

			// Render settings
			if (ImGui::CollapsingHeader("Settings"))
			{
				// Enable/disable cosine weighted diffuse reflections
				ImGui::Checkbox("Cosine weighted diffuse", &inst->settings.cosineWeightedDiffuseReflection);
				ImGui::Checkbox("Russian roulette", &inst->settings.russianRoulette);

				// Sky color and intensity
				ImGui::ColorEdit3("Sky color", &inst->settings.skyColor[0], ImGuiColorEditFlags_DisplayRGB);
				ImGui::DragFloat("Sky color intensity", &inst->settings.skyColorIntensity, 0.001f);

				// Enable/disable conversion of final color to SRGB
				ImGui::Checkbox("Linear to SRGB", &inst->settings.linearToSRGB);
			}
		}
		ImGui::End();
	}

}

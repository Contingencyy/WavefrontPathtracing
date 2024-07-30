#include "Pch.h"
#include "CPUPathtracer.h"
#include "RaytracingUtils.h"
#include "DX12/DX12Backend.h"
#include "Renderer/RendererCommon.h"
#include "Renderer/ResourceSlotmap.h"
#include "Renderer/AccelerationStructure/BVH.h"
#include "Renderer/AccelerationStructure/BVHInstance.h"
#include "Renderer/AccelerationStructure/TLAS.h"
#include "Core/Camera/CameraController.h"
#include "Core/Logger.h"
#include "Core/Random.h"
#include "Core/Threadpool.h"
#include "Core/Profiling/Profiler.h"
#include "Core/Profiling/CPUTimer.h"

#include "imgui/imgui.h"

using namespace Renderer;

namespace CPUPathtracer
{

	struct Instance
	{
		ResourceSlotmap<RenderTextureHandle, Texture> textureMap;
		ResourceSlotmap<RenderMeshHandle, BVH> BVHMap;
		
		std::vector<BVHInstance> BVHInstances;
		std::vector<Material> BVHMaterials;
		TLAS sceneTLAS;
		Camera sceneCamera;
		Texture* sceneHDREnvTexture;

		uint32_t renderWidth = 0;
		uint32_t renderHeight = 0;
		float invRenderWidth = 0.0f;
		float invRenderHeight = 0.0f;

		std::vector<uint32_t> pixelBuffer;
		std::vector<glm::vec4> pixelAccumulator;
		double energyAccumulator = 0.0;
		uint32_t numAccumulatedFrames = 0;

		Threadpool renderThreadpool;

		uint64_t frameIndex = 0;

		RenderSettings settings;
	} static *inst;

	static void ResetAccumulation()
	{
		// Clear the accumulator, reset accumulated frame count
		std::fill(inst->pixelAccumulator.begin(), inst->pixelAccumulator.end(), glm::vec4(0.0f));
		inst->energyAccumulator = 0.0;
		inst->numAccumulatedFrames = 0;
	}

	static glm::vec3 SampleHDREnvironment(const glm::vec3& dir)
	{
		glm::vec2 uv = RTUtil::DirectionToEquirectangularUV(dir);
		uv.y = glm::abs(uv.y - 1.0f);

		glm::uvec2 texelPos = uv * glm::vec2(inst->sceneHDREnvTexture->width, inst->sceneHDREnvTexture->height);
		glm::vec4 color = inst->sceneHDREnvTexture->pixelData[texelPos.y * inst->sceneHDREnvTexture->width + texelPos.x];

		return color.xyz;
	}

	static glm::vec3 ApplyPostProcessing(const glm::vec3& color)
	{
		glm::vec3 finalColor = color;

		if (inst->settings.renderVisualization != RenderVisualization_None)
		{
			if (inst->settings.renderVisualization == RenderVisualization_HitAlbedo ||
				inst->settings.renderVisualization == RenderVisualization_HitEmissive ||
				inst->settings.renderVisualization == RenderVisualization_HitAbsorption)
				return RTUtil::LinearToSRGB(finalColor);
			else
				return finalColor;
		}

		// Apply exposure
		finalColor *= inst->settings.postfx.exposure;

		// Contrast & Brightness
		finalColor = RTUtil::AdjustContrastBrightness(finalColor, inst->settings.postfx.contrast, inst->settings.postfx.brightness);

		// Saturation
		finalColor = RTUtil::Saturate(finalColor, inst->settings.postfx.saturation);

		// Apply simple tonemapper
		finalColor = RTUtil::TonemapReinhardWhite(finalColor.xyz, inst->settings.postfx.maxWhite);

		// Convert final color from linear to SRGB, if enabled, and if we do not use any render data visualization
		if (inst->settings.postfx.linearToSRGB)
			finalColor.xyz = RTUtil::LinearToSRGB(finalColor.xyz);

		return finalColor;
	}

	static glm::vec4 TracePath(Ray& ray)
	{
		// REMEMBER: Set DXC to not use legacy struct padding
		// FIX: Scaling a BVH instance is broken
		// TODO: Normal per vertex, interpolate with UV coordinates
		// TOOD: DX12 Agility SDK & Enhanced Barriers
		// TODO: HDR environment maps
		// TODO: Application window for profiler, Timer avg/min/max
		// TODO: Profiling for TLAS builds
		// TODO: BVH bounding box visualization
		// TODO: Next event estimation
		// TODO: Stop moving mouse back to center when window loses focus
		// TODO: Profile BVH build times for the BLAS and also the TLAS
		
		// TODO: Make any resolution work with the multi-threaded rendering dispatch
		// TODO: Setting for accumulation should be a render setting, with defaults for each render visualization
		// TODO: BVH Refitting and Rebuilding (https://jacco.ompf2.com/2022/04/26/how-to-build-a-bvh-part-4-animation/)
		// TODO: Display BVH build data like max depth, total number of vertices/triangles, etc. in the mesh assets once I have menus for that
		// TODO: Separate BVH Builder and BVH's, which should just contain the actual data after a finished BVH build
		// TODO: Assets, loading and storing BVH's
		// TODO: DOF (https://youtu.be/Qz0KTGYJtUk)
		// TODO: GPU Pathtracer
			// NOTE: Need to figure out how the application-renderer interface should look like, the application should not know and/or care
			//		 whether the frame was rendered using the GPU or CPU pathtracer
		// FIX: Sometimes the colors just get flat and not even resetting the accumulator fixes it??
		// TODO: Transmittance and density instead of absorption?
		// TODO: Total energy received, accumulated over time so it slowly stabilizes, for comparisons
			// NOTE: Calculate average by using previous frame value * numAccumulated - 1 and current frame just times 1
			// FIX: Why does the amount of average energy received change when we toggle inst->settings.linearToSRGB?
		// TODO: Window resizing and resolution resizing
		// TODO: Tooltips for render data visualization modes to know what they do, useful for e.g. ray recursion depth or RR kill depth
		// TODO: Spawn new objects from ImGui
		// TODO: Scene hierarchy
		// TODO: ImGuizmo to transform objects
		// TODO: Ray/path visualization mode
		// TODO: Unit tests for RTUtil functions like UniformHemisphereSampling (testing the resulting dir for length 1 for example)
		// TODO: Physically-based rendering
		// TODO: BRDF importance sampling
		// TODO: Denoising
		// TODO: ReSTIR

		glm::vec3 throughput(1.0f);
		glm::vec3 energy(0.0f);

		bool isSpecularRay = false;
		uint32_t currRayDepth = 0;
		bool survivedRR = true;

		while (currRayDepth <= inst->settings.rayMaxRecursionDepth)
		{
			HitResult hit = inst->sceneTLAS.TraceRay(ray);

			// Add sky color to energy if we have not hit an object, and terminate path
			if (!hit.HasHitGeometry())
			{
				//energy += inst->settings.skyColor * inst->settings.skyColorIntensity * throughput;
				energy += SampleHDREnvironment(ray.dir) * throughput;
				break;
			}

			Material hitMaterial = inst->BVHMaterials[hit.instanceIdx];

			// Handle any render data visualization that can happen after a single trace
			if (inst->settings.renderVisualization != RenderVisualization_None)
			{
				bool stopTracingPath = false;

				switch (inst->settings.renderVisualization)
				{
				case RenderVisualization_HitAlbedo:					 energy = hitMaterial.albedo; stopTracingPath = true; break;
				case RenderVisualization_HitNormal:					 energy = glm::abs(hit.normal); stopTracingPath = true; break;
				case RenderVisualization_HitSpecRefract:			 energy = glm::vec3(hitMaterial.specular, hitMaterial.refractivity, 0.0f); stopTracingPath = true; break;
				case RenderVisualization_HitAbsorption:				 energy = glm::vec3(hitMaterial.absorption); stopTracingPath = true; break;
				case RenderVisualization_HitEmissive:				 energy = glm::vec3(hitMaterial.emissive * hitMaterial.intensity * static_cast<float>(hitMaterial.isEmissive)); stopTracingPath = true; break;
				case RenderVisualization_Depth:						 energy = glm::vec3(hit.t * 0.01f); stopTracingPath = true; break;
				case RenderVisualization_AccelerationStructureDepth: energy = glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), ray.bvhDepth / 50.0f); stopTracingPath = true; break;
				}

				if (stopTracingPath)
					break;
			}

			// If the surface is emissive, we simply add its color to the energy output and stop tracing
			if (hitMaterial.isEmissive)
			{
				energy += hitMaterial.emissive * hitMaterial.intensity * throughput;
				break;
			}

			// Russian roulette - Stop tracing the path with a probability that is based on the material albedo
			// Since the throughput is gonna be dependent on the albedo, dark albedos carry less energy to the eye,
			// so we can eliminate those paths with a higher probability and safe ourselves the time continuing tracing low impact paths.
			// Need to adjust the throughput if the path survives since that path should carry a higher weight based on its survival probability.
			if (inst->settings.russianRoulette)
			{
				float survivalProbability = RTUtil::SurvivalProbabilityRR(hitMaterial.albedo);
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
			if (r < hitMaterial.specular)
			{
				glm::vec3 specularDir = RTUtil::Reflect(ray.dir, hit.normal);
				ray = Ray(hit.pos + specularDir * RAY_NUDGE_MODIFIER, specularDir);

				throughput *= hitMaterial.albedo;
				isSpecularRay = true;
			}
			// Refraction path
			else if (r < hitMaterial.specular + hitMaterial.refractivity)
			{
				glm::vec3 N = hit.normal;

				float cosi = glm::clamp(glm::dot(N, ray.dir), -1.0f, 1.0f);
				float etai = 1.0f, etat = hitMaterial.ior;

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
						throughput *= hitMaterial.albedo;

						if (inside)
						{
							glm::vec3 absorption = glm::vec3(
								glm::exp(-hitMaterial.absorption.x * ray.t),
								glm::exp(-hitMaterial.absorption.y * ray.t),
								glm::exp(-hitMaterial.absorption.z * ray.t)
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

						throughput *= hitMaterial.albedo;
						isSpecularRay = true;
					}
				}
			}
			// Diffuse path
			else
			{
				glm::vec3 diffuseBRDF = hitMaterial.albedo * INV_PI;
				glm::vec3 diffuseDir(0.0f);

				float NdotR = 0.0f;
				float hemispherePDF = 0.0f;

				// Cosine-weighted diffuse reflection
				if (inst->settings.cosineWeightedDiffuseReflection)
				{
					diffuseDir = RTUtil::CosineWeightedHemisphereSample(hit.normal);
					NdotR = glm::dot(diffuseDir, hit.normal);
					hemispherePDF = NdotR * INV_PI;
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
		if (inst->settings.renderVisualization != RenderVisualization_None)
		{
			switch (inst->settings.renderVisualization)
			{
			case RenderVisualization_RayRecursionDepth:
			{
				energy = glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), currRayDepth / static_cast<float>(inst->settings.rayMaxRecursionDepth));
			} break;
			case RenderVisualization_RussianRouletteKillDepth:
			{
				float weight = glm::clamp((currRayDepth / static_cast<float>(inst->settings.rayMaxRecursionDepth)) - static_cast<float>(survivedRR), 0.0f, 1.0f);
				energy = glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), weight);
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
		LOG_INFO("CPUPathtracer", "Exit");
		
		DX12Backend::Exit();

		delete inst;
	}

	void BeginFrame()
	{
		DX12Backend::BeginFrame();

		if (inst->settings.renderVisualization != RenderVisualization_None)
			ResetAccumulation();
	}

	void EndFrame()
	{
		DX12Backend::EndFrame();
		DX12Backend::CopyToBackBuffer(reinterpret_cast<char*>(inst->pixelBuffer.data()));
		DX12Backend::Present();

		inst->frameIndex++;
	}

	void BeginScene(const Camera& sceneCamera, RenderTextureHandle hdrEnvHandle)
	{
		if (inst->sceneCamera.viewMatrix != sceneCamera.viewMatrix)
			ResetAccumulation();

		inst->sceneCamera = sceneCamera;
		inst->sceneHDREnvTexture = inst->textureMap.Find(hdrEnvHandle);
		ASSERT(inst->sceneHDREnvTexture);
	}

	void Render()
	{
		PROFILE_SCOPE(GlobalProfilingScope_RenderTime);
		inst->numAccumulatedFrames++;

		// Build the current scene's TLAS
		inst->sceneTLAS = TLAS();
		inst->sceneTLAS.Build(inst->BVHInstances);

		float aspectRatio = inst->renderWidth / static_cast<float>(inst->renderHeight);
		float tanFOV = glm::tan(glm::radians(inst->sceneCamera.vfov) / 2.0f);
		
		uint32_t numPixels = inst->renderWidth * inst->renderHeight;
		double invNumPixels = 1.0f / static_cast<float>(numPixels);

		glm::uvec2 dispatchDim = { 16, 16 };
		ASSERT(inst->renderWidth % dispatchDim.x == 0);
		ASSERT(inst->renderHeight % dispatchDim.y == 0);

		auto tracePathThroughPixelJob = [&dispatchDim, &aspectRatio, &tanFOV, &invNumPixels](Threadpool::JobDispatchArgs dispatchArgs) {
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
					glm::vec4 tracedColor = TracePath(ray);

					// Update accumulator
					uint32_t pixelPos = y * inst->renderWidth + x;
					inst->pixelAccumulator[pixelPos] += tracedColor;
					inst->energyAccumulator += static_cast<double>(tracedColor.x + tracedColor.y + tracedColor.z) * invNumPixels;

					// Determine final color for pixel
					glm::vec4 finalColor = inst->pixelAccumulator[pixelPos] /
						static_cast<float>(inst->numAccumulatedFrames);

					// Apply post-processing stack to final color
					finalColor.xyz = ApplyPostProcessing(finalColor.xyz);

					// Write final color
					inst->pixelBuffer[pixelPos] = RTUtil::Vec4ToUint32(finalColor);
				}
			}
		};

		uint32_t numJobs = numPixels / (dispatchDim.x * dispatchDim.y);

		inst->renderThreadpool.Dispatch(numJobs, 16, tracePathThroughPixelJob);
		inst->renderThreadpool.WaitAll();
	}

	void EndScene()
	{
		inst->BVHInstances.clear();
		inst->BVHMaterials.clear();
	}

	void RenderUI()
	{
		if (ImGui::Begin("Renderer"))
		{
			bool resetAccumulator = false;

			ImGui::Text("Render time: %.3f ms", Profiler::GetTimerResult(GlobalProfilingScope_RenderTime).lastElapsed * 1000.0f);

			ImGui::Text("Resolution: %ux%u", inst->renderWidth, inst->renderHeight);
			ImGui::Text("Accumulated frames: %u", inst->numAccumulatedFrames);
			ImGui::Text("Total energy received: %.3f", inst->energyAccumulator / static_cast<double>(inst->numAccumulatedFrames) * 1000.0f);

			// Debug category
			if (ImGui::CollapsingHeader("Debug"))
			{
				ImGui::Indent(10.0f);

				// Render data visualization
				ImGui::Text("Render data visualization mode");
				if (ImGui::BeginCombo("##Render data visualization mode", RenderDataVisualizationLabels[inst->settings.renderVisualization].c_str(), ImGuiComboFlags_None))
				{
					for (uint32_t i = 0; i < RenderVisualization_Count; ++i)
					{
						bool is_selected = i == inst->settings.renderVisualization;

						if (ImGui::Selectable(RenderDataVisualizationLabels[i].c_str(), is_selected))
						{
							inst->settings.renderVisualization = (RenderVisualization)i;
							resetAccumulator = true;
						}

						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::Unindent(10.0f);
			}

			// Render settings
			if (ImGui::CollapsingHeader("Settings"))
			{
				ImGui::Indent(10.0f);

				if (ImGui::SliderInt("Max Ray Recursion Depth: %u", reinterpret_cast<int32_t*>(&inst->settings.rayMaxRecursionDepth), 0, 8)) resetAccumulator = true;

				// Enable/disable cosine weighted diffuse reflections, russian roulette
				if (ImGui::Checkbox("Cosine weighted diffuse", &inst->settings.cosineWeightedDiffuseReflection)) resetAccumulator = true;
				if (ImGui::Checkbox("Russian roulette", &inst->settings.russianRoulette)) resetAccumulator = true;

				// Sky color and intensity
				if (ImGui::ColorEdit3("Sky color", &inst->settings.skyColor[0], ImGuiColorEditFlags_DisplayRGB)) resetAccumulator = true;
				if (ImGui::DragFloat("Sky color intensity", &inst->settings.skyColorIntensity, 0.001f)) resetAccumulator = true;

				// Post-process settings, constract, brightness, saturation, sRGB
				ImGui::SetNextItemOpen(true, ImGuiCond_Once);
				if (ImGui::CollapsingHeader("Post-processing"))
				{
					ImGui::Indent(10.0f);

					if (ImGui::SliderFloat("Max white", &inst->settings.postfx.maxWhite, 0.0f, 100.0f)) resetAccumulator = true;
					if (ImGui::SliderFloat("Exposure", &inst->settings.postfx.exposure, 0.0f, 100.0f)) resetAccumulator = true;
					if (ImGui::SliderFloat("Contrast", &inst->settings.postfx.contrast, 0.0f, 3.0f)) resetAccumulator = true;
					if (ImGui::SliderFloat("Brightness", &inst->settings.postfx.brightness, -1.0f, 1.0f)) resetAccumulator = true;
					if (ImGui::SliderFloat("Saturation", &inst->settings.postfx.saturation, 0.0f, 10.0f)) resetAccumulator = true;
					if (ImGui::Checkbox("Linear to SRGB", &inst->settings.postfx.linearToSRGB)) resetAccumulator = true;

					ImGui::Unindent(10.0f);
				}

				ImGui::Unindent(10.0f);
			}

			if (resetAccumulator)
				ResetAccumulation();
		}
		ImGui::End();
	}

	RenderTextureHandle CreateTexture(uint32_t textureWidth, uint32_t textureHeight, const std::vector<glm::vec4>& pixelData)
	{
		Texture texture = {};
		texture.width = textureWidth;
		texture.height = textureHeight;
		texture.pixelData = pixelData;

		RenderTextureHandle textureHandle = inst->textureMap.Add(std::move(texture));
		return textureHandle;
	}

	RenderMeshHandle CreateMesh(const std::span<Vertex>& vertices, const std::span<uint32_t>& indices)
	{
		BVH::BuildOptions buildOptions = {};

		BVH meshBVH = {};
		meshBVH.Build(vertices, indices, buildOptions);

		RenderMeshHandle meshHandle = inst->BVHMap.Add(std::move(meshBVH));
		return meshHandle;
	}

	void SubmitMesh(RenderMeshHandle renderMeshHandle, const glm::mat4& transform, const Material& material)
	{
		const BVH* bvh = inst->BVHMap.Find(renderMeshHandle);
		if (!bvh)
			return;

		BVHInstance instance(bvh);
		instance.SetTransform(transform);

		inst->BVHInstances.push_back(std::move(instance));
		inst->BVHMaterials.push_back(std::move(material));
	}

}

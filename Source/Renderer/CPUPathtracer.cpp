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

#include "imgui/imgui.h"

using namespace Renderer;

namespace CPUPathtracer
{

	struct Instance
	{
		MemoryArena Arena;
		u8* ArenaMarkerFrame;

		u32 RenderWidth;
		u32 RenderHeight;
		f32 InvRenderWidth;
		f32 InvRenderHeight;

		u32* PixelBuffer;
		glm::vec4* PixelAccumulator;
		f64 AvgEnergyAccumulator;
		u32 AccumulatedFrameCount;

		ResourceSlotmap<RenderTextureHandle, Texture> TextureSlotmap;
		ResourceSlotmap<RenderMeshHandle, BVH> BvhSlotmap;
		
		u32 BVHInstanceCount;
		u32 BVHInstanceAt;
		BVHInstance* BVHInstances;
		Material* BVHMaterials;

		TLAS SceneTLAS;
		Camera SceneCamera;
		Texture* SceneHdrEnv;

		Threadpool RenderThreadpool;
		uint64_t FrameIndex;
		RenderSettings Settings;
	} static *Inst;

	static void ResetAccumulation()
	{
		// Clear the accumulator, reset accumulated frame count
		memset(Inst->PixelAccumulator, 0, Inst->RenderWidth * Inst->RenderHeight * sizeof(Inst->PixelAccumulator[0]));
		Inst->AvgEnergyAccumulator = 0.0;
		Inst->AccumulatedFrameCount = 0;
	}

	static glm::vec3 SampleHDREnvironment(const glm::vec3& Dir)
	{
		glm::vec2 uv = RTUtil::DirectionToEquirectangularUV(Dir);
		uv.y = glm::abs(uv.y - 1.0f);

		glm::uvec2 TexelPos = uv * glm::vec2(Inst->SceneHdrEnv->Width, Inst->SceneHdrEnv->Height);

		glm::vec4 Color = {};
		Color.r = Inst->SceneHdrEnv->PtrPixelData[TexelPos.y * Inst->SceneHdrEnv->Width * 4 + TexelPos.x * 4];
		Color.g = Inst->SceneHdrEnv->PtrPixelData[TexelPos.y * Inst->SceneHdrEnv->Width * 4 + TexelPos.x * 4 + 1];
		Color.b = Inst->SceneHdrEnv->PtrPixelData[TexelPos.y * Inst->SceneHdrEnv->Width * 4 + TexelPos.x * 4 + 2];
		Color.a = Inst->SceneHdrEnv->PtrPixelData[TexelPos.y * Inst->SceneHdrEnv->Width * 4 + TexelPos.x * 4 + 3];

		return Color.xyz;
	}

	static glm::vec3 ApplyPostProcessing(const glm::vec3& Color)
	{
		glm::vec3 FinalColor = Color;

		if (Inst->Settings.RenderVisualization != RenderVisualization_None)
		{
			if (Inst->Settings.RenderVisualization == RenderVisualization_HitAlbedo ||
				Inst->Settings.RenderVisualization == RenderVisualization_HitEmissive ||
				Inst->Settings.RenderVisualization == RenderVisualization_HitAbsorption)
				return RTUtil::LinearToSRGB(FinalColor);
			else
				return FinalColor;
		}

		// Apply exposure
		FinalColor *= Inst->Settings.Postfx.Exposure;

		// Contrast & Brightness
		FinalColor = RTUtil::AdjustContrastBrightness(FinalColor, Inst->Settings.Postfx.Contrast, Inst->Settings.Postfx.Brightness);

		// Saturation
		FinalColor = RTUtil::Saturate(FinalColor, Inst->Settings.Postfx.Saturation);

		// Apply simple tonemapper
		FinalColor = RTUtil::TonemapReinhardWhite(FinalColor.xyz, Inst->Settings.Postfx.MaxWhite);

		// Convert final color from linear to SRGB, if enabled, and if we do not use any render data visualization
		if (Inst->Settings.Postfx.bLinearToSRGB)
			FinalColor.xyz = RTUtil::LinearToSRGB(FinalColor.xyz);

		return FinalColor;
	}

	static glm::vec4 TracePath(Ray& Ray)
	{
		// TODO: Make memory arena statistics and visualizations using Dear ImPlot library
		// TODO: Custom string class, counted strings, use in Assertion.h, Logger.h, EntryPoint.cpp for command line parsing
		// TODO: SafeTruncate for int and uint types, asserting if the value of a u64 is larger than the one to truncate to
		// TODO: Make any resolution work with the multi-threaded rendering dispatch
		// TODO: Find a better epsilon for the Möller-Trumbore Triangle Intersection Algorithm
		// TODO: Add Config.h which has a whole bunch of defines like which intersection algorithms to use and what not
		// REMEMBER: Set DXC to not use legacy struct padding
		// TOOD: DX12 Agility SDK & Enhanced Barriers
		// TODO: Profiling
		// TODO: Application window for profiler, Timer avg/min/max
		// TODO: Profiling for TLAS builds
		// TODO: Next event estimation
		// TODO: Stop moving mouse back to Center when window loses focus
		// TODO: Profile BVH build times for the BLAS and also the TLAS
		
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
		// TODO: Transmittance and density instead of Absorption?
		// TODO: Total Energy received, accumulated over time so it slowly stabilizes, for comparisons
			// NOTE: Calculate average by using previous frame value * numAccumulated - 1 and current frame just times 1
			// FIX: Why does the amount of average Energy received change when we toggle Inst->Settings.linearToSRGB?
		// TODO: Window resizing and resolution resizing
		// TODO: Tooltips for render data visualization modes to know what they do, useful for e.g. ray recursion depth or RR kill depth
		// TODO: Scene hierarchy
			// TODO: Spawn new objects from ImGui
		// TODO: ImGuizmo to transform objects
		// TODO: Ray/path visualization mode
		// TODO: Unit tests for RTUtil functions like UniformHemisphereSampling (testing the resulting Dir for length 1 for example)
		// TODO: Physically-based rendering
		// TODO: BRDF importance sampling
		// TODO: Denoising
		// TODO: ReSTIR

		glm::vec3 Throughput(1.0f);
		glm::vec3 Energy(0.0f);

		u32 RayDepth = 0;
		b8 bSpecularRay = false;
		b8 bSurvivedRR = true;

		while (RayDepth <= Inst->Settings.RayMaxRecursionDepth)
		{
			HitResult Hit = {};
			Inst->SceneTLAS.TraceRay(Ray, Hit);

			// Render visualizations that are not depending on valid geometry data
			if (Inst->Settings.RenderVisualization != RenderVisualization_None)
			{
				b8 StopTrace = false;

				switch (Inst->Settings.RenderVisualization)
				{
					case RenderVisualization_AccelerationStructureDepth: Energy = glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), Ray.BvhDepth / 50.0f); StopTrace = true; break;
				}

				if (StopTrace)
					break;
			}

			// Add sky color to Energy if we have not Hit an object, and terminate path
			if (!Hit.HasHitGeometry())
			{
				Energy += Inst->Settings.HdrEnvIntensity * SampleHDREnvironment(Ray.Dir) * Throughput;
				break;
			}

			Material HitMat = Inst->BVHMaterials[Hit.InstanceIdx];

			// Render visualizations that are depending on valid geometry data
			if (Inst->Settings.RenderVisualization != RenderVisualization_None)
			{
				b8 StopTrace = false;

				switch (Inst->Settings.RenderVisualization)
				{
				case RenderVisualization_HitAlbedo:		  Energy = HitMat.Albedo; StopTrace = true; break;
				case RenderVisualization_HitNormal:		  Energy = glm::abs(Hit.Normal); StopTrace = true; break;
				case RenderVisualization_HitBarycentrics: Energy = Hit.Bary; StopTrace = true; break;
				case RenderVisualization_HitSpecRefract:  Energy = glm::vec3(HitMat.Specular, HitMat.Refractivity, 0.0f); StopTrace = true; break;
				case RenderVisualization_HitAbsorption:	  Energy = glm::vec3(HitMat.Absorption); StopTrace = true; break;
				case RenderVisualization_HitEmissive:	  Energy = glm::vec3(HitMat.Emissive * HitMat.Intensity * static_cast<f32>(HitMat.bEmissive)); StopTrace = true; break;
				case RenderVisualization_Depth:			  Energy = glm::vec3(Hit.t * 0.01f); StopTrace = true; break;
				}

				if (StopTrace)
					break;
			}

			// If the surface is Emissive, we simply add its color to the Energy output and stop tracing
			if (HitMat.bEmissive)
			{
				Energy += HitMat.Emissive * HitMat.Intensity * Throughput;
				break;
			}

			// Russian roulette - Stop tracing the path with a probability that is based on the material Albedo
			// Since the Throughput is gonna be dependent on the Albedo, dark albedos carry less Energy to the eye,
			// so we can eliminate those paths with a higher probability and safe ourselves the time continuing tracing low impact paths.
			// Need to adjust the Throughput if the path survives since that path should carry a higher weight based on its survival probability.
			if (Inst->Settings.bRussianRoulette)
			{
				f32 RRSurvivalProbability = RTUtil::SurvivalProbabilityRR(HitMat.Albedo);
				if (RRSurvivalProbability < Random::Float())
				{
					bSurvivedRR = false;
					break;
				}
				else
				{
					Throughput *= 1.0f / RRSurvivalProbability;
				}
			}

			// Get a random 0..1 range Float to determine which path to take
			f32 r = Random::Float();

			// Specular path
			if (r < HitMat.Specular)
			{
				glm::vec3 SpecularDir = RTUtil::Reflect(Ray.Dir, Hit.Normal);
				Ray = MakeRay(Hit.Pos + SpecularDir * RAY_NUDGE_MODIFIER, SpecularDir);

				Throughput *= HitMat.Albedo;
				bSpecularRay = true;
			}
			// Refraction path
			else if (r < HitMat.Specular + HitMat.Refractivity)
			{
				glm::vec3 N = Hit.Normal;

				f32 cosi = glm::clamp(glm::dot(N, Ray.Dir), -1.0f, 1.0f);
				f32 etai = 1.0f, etat = HitMat.Ior;

				f32 Fr = 1.0f;
				b8 bInside = true;

				// Figure out if we are bInside or outside of the object we've Hit
				if (cosi < 0.0f)
				{
					cosi = -cosi;
					bInside = false;
				}
				else
				{
					std::swap(etai, etat);
					N = -N;
				}

				f32 eta = etai / etat;
				f32 k = 1.0f - eta * eta * (1.0f - cosi * cosi);

				// Follow refraction or Specular path
				if (k >= 0.0f)
				{
					glm::vec3 RefractDir = RTUtil::Refract(Ray.Dir, N, eta, cosi, k);

					f32 cosIn = glm::dot(Ray.Dir, Hit.Normal);
					f32 cosOut = glm::dot(RefractDir, Hit.Normal);

					Fr = RTUtil::Fresnel(cosIn, cosOut, etai, etat);

					if (Random::Float() > Fr)
					{
						Throughput *= HitMat.Albedo;

						if (bInside)
						{
							glm::vec3 absorption = glm::vec3(
								glm::exp(-HitMat.Absorption.x * Ray.t),
								glm::exp(-HitMat.Absorption.y * Ray.t),
								glm::exp(-HitMat.Absorption.z * Ray.t)
							);
							Throughput *= absorption;
						}

						Ray = MakeRay(Hit.Pos + RefractDir * RAY_NUDGE_MODIFIER, RefractDir);
						bSpecularRay = true;
					}
					else
					{
						glm::vec3 specularDir = RTUtil::Reflect(Ray.Dir, Hit.Normal);
						Ray = MakeRay(Hit.Pos + specularDir * RAY_NUDGE_MODIFIER, specularDir);

						Throughput *= HitMat.Albedo;
						bSpecularRay = true;
					}
				}
			}
			// Diffuse path
			else
			{
				glm::vec3 DiffuseBrdf = HitMat.Albedo * INV_PI;
				glm::vec3 DiffuseDir(0.0f);

				f32 NdotR = 0.0f;
				f32 HemispherePDF = 0.0f;

				// Cosine-weighted diffuse reflection
				if (Inst->Settings.bCosineWeightedDiffuseReflection)
				{
					DiffuseDir = RTUtil::CosineWeightedHemisphereSample(Hit.Normal);
					NdotR = glm::dot(DiffuseDir, Hit.Normal);
					HemispherePDF = NdotR * INV_PI;
				}
				// Uniform hemisphere diffuse reflection
				else
				{
					DiffuseDir = RTUtil::UniformHemisphereSample(Hit.Normal);
					NdotR = glm::dot(DiffuseDir, Hit.Normal);
					HemispherePDF = INV_TWO_PI;
				}

				Ray = MakeRay(Hit.Pos + DiffuseDir * RAY_NUDGE_MODIFIER, DiffuseDir);
				Throughput *= (NdotR * DiffuseBrdf) / HemispherePDF;
				bSpecularRay = false;
			}

			RayDepth++;
		}

		// Handle any render data visualization that needs to trace the full path first
		if (Inst->Settings.RenderVisualization != RenderVisualization_None)
		{
			switch (Inst->Settings.RenderVisualization)
			{
			case RenderVisualization_RayRecursionDepth:
			{
				Energy = glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), RayDepth / static_cast<f32>(Inst->Settings.RayMaxRecursionDepth));
			} break;
			case RenderVisualization_RussianRouletteKillDepth:
			{
				f32 Weight = glm::clamp((RayDepth / static_cast<f32>(Inst->Settings.RayMaxRecursionDepth)) - static_cast<f32>(bSurvivedRR), 0.0f, 1.0f);
				Energy = glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), Weight);
			} break;
			}
		}

		return glm::vec4(Energy, 1.0f);
	}

	void Init(u32 RenderWidth, u32 RenderHeight)
	{
		LOG_INFO("CPUPathtracer", "Init");
		
		// We first allocate the instance on the passed Arena, then we allocate from the instance arena
		Inst = ARENA_BOOTSTRAP(Instance, 0);

		Inst->RenderWidth = RenderWidth;
		Inst->RenderHeight = RenderHeight;
		Inst->InvRenderWidth = 1.0f / static_cast<f32>(Inst->RenderWidth);
		Inst->InvRenderHeight = 1.0f / static_cast<f32>(Inst->RenderHeight);

		Inst->PixelBuffer = ARENA_ALLOC_ARRAY_ZERO(&Inst->Arena, u32, Inst->RenderWidth * Inst->RenderHeight);
		Inst->PixelAccumulator = ARENA_ALLOC_ARRAY_ZERO(&Inst->Arena, glm::vec4, Inst->RenderWidth * Inst->RenderHeight);

		Inst->TextureSlotmap.Init(&Inst->Arena);
		Inst->BvhSlotmap.Init(&Inst->Arena);

		Inst->BVHInstanceCount = 100;
		Inst->BVHInstanceAt = 0;
		Inst->BVHInstances = ARENA_ALLOC_ARRAY_ZERO(&Inst->Arena, BVHInstance, Inst->BVHInstanceCount);
		Inst->BVHMaterials = ARENA_ALLOC_ARRAY_ZERO(&Inst->Arena, Material, Inst->BVHInstanceCount);

		Inst->RenderThreadpool.Init(&Inst->Arena);

		// Default render settings
		Inst->Settings.RenderVisualization = RenderVisualization_None;
		Inst->Settings.RayMaxRecursionDepth = 8;

		Inst->Settings.bCosineWeightedDiffuseReflection = true;
		Inst->Settings.bRussianRoulette = true;

		Inst->Settings.HdrEnvIntensity = 1.0f;

		Inst->Settings.Postfx.MaxWhite = 10.0f;
		Inst->Settings.Postfx.Exposure = 1.0f;
		Inst->Settings.Postfx.Contrast = 1.0f;
		Inst->Settings.Postfx.Brightness = 0.0f;
		Inst->Settings.Postfx.Saturation = 1.0f;
		Inst->Settings.Postfx.bLinearToSRGB = true;

		DX12Backend::Init();
	}

	void Exit()
	{
		LOG_INFO("CPUPathtracer", "Exit");
		
		DX12Backend::Exit();

		Inst->TextureSlotmap.Destroy();
		Inst->BvhSlotmap.Destroy();
		Inst->RenderThreadpool.Destroy();
	}

	void BeginFrame()
	{
		DX12Backend::BeginFrame();

		if (Inst->Settings.RenderVisualization != RenderVisualization_None)
			ResetAccumulation();

		Inst->ArenaMarkerFrame = Inst->Arena.PtrAt;
	}

	void EndFrame()
	{
		DX12Backend::EndFrame();
		DX12Backend::CopyToBackBuffer(reinterpret_cast<char*>(Inst->PixelBuffer));
		DX12Backend::Present();

		Inst->FrameIndex++;

		ARENA_FREE(&Inst->Arena, Inst->ArenaMarkerFrame);
	}

	void BeginScene(const Camera& SceneCamera, RenderTextureHandle REnvTextureHandle)
	{
		if (Inst->SceneCamera.ViewMatrix != SceneCamera.ViewMatrix)
			ResetAccumulation();

		Inst->SceneCamera = SceneCamera;
		Inst->SceneHdrEnv = Inst->TextureSlotmap.Find(REnvTextureHandle);
		ASSERT(Inst->SceneHdrEnv);
	}

	void Render()
	{
		Inst->AccumulatedFrameCount++;

		// Build the current scene's TLAS
		Inst->SceneTLAS.Build(&Inst->Arena, Inst->BVHInstances, Inst->BVHInstanceAt);

		f32 AspectRatio = Inst->RenderWidth / static_cast<f32>(Inst->RenderHeight);
		f32 TanFOV = glm::tan(glm::radians(Inst->SceneCamera.VFovDeg) / 2.0f);
		
		u32 PixelCount = Inst->RenderWidth * Inst->RenderHeight;
		f64 InvPixelCount = 1.0f / static_cast<f32>(PixelCount);

		glm::uvec2 DispatchDim = { 16, 16 };
		ASSERT(Inst->RenderWidth % DispatchDim.x == 0);
		ASSERT(Inst->RenderHeight % DispatchDim.y == 0);

		auto TracePathThroughPixelJob = [&DispatchDim, &AspectRatio, &TanFOV, &InvPixelCount](Threadpool::JobDispatchArgs dispatchArgs) {
			const u32 DispatchX = (dispatchArgs.JobIndex * DispatchDim.x) % Inst->RenderWidth;
			const u32 DispatchY = ((dispatchArgs.JobIndex * DispatchDim.x) / Inst->RenderWidth) * DispatchDim.y;

			for (u32 y = DispatchY; y < DispatchY + DispatchDim.y; ++y)
			{
				for (u32 x = DispatchX; x < DispatchX + DispatchDim.x; ++x)
				{
					// Construct primary ray from camera through the current pixel
					Ray Ray = RTUtil::ConstructCameraRay(Inst->SceneCamera, x, y, TanFOV,
						AspectRatio, Inst->InvRenderWidth, Inst->InvRenderHeight);

					// Start tracing path through pixel
					glm::vec4 PathColor = TracePath(Ray);

					// Update accumulator
					u32 PixelPos = y * Inst->RenderWidth + x;
					Inst->PixelAccumulator[PixelPos] += PathColor;
					Inst->AvgEnergyAccumulator += static_cast<f64>(PathColor.x + PathColor.y + PathColor.z) * InvPixelCount;

					// Determine final color for pixel
					glm::vec4 FinalColor = Inst->PixelAccumulator[PixelPos] /
						static_cast<f32>(Inst->AccumulatedFrameCount);

					// Apply post-processing stack to final color
					FinalColor.xyz = ApplyPostProcessing(FinalColor.xyz);

					// Write final color
					Inst->PixelBuffer[PixelPos] = RTUtil::Vec4ToUint32(FinalColor);
				}
			}
		};

		u32 JobCount = PixelCount / (DispatchDim.x * DispatchDim.y);

		Inst->RenderThreadpool.Dispatch(JobCount, 16, TracePathThroughPixelJob);
		Inst->RenderThreadpool.WaitAll();
	}

	void EndScene()
	{
		Inst->BVHInstanceAt = 0;
	}

	void RenderUI()
	{
		if (ImGui::Begin("Renderer"))
		{
			b8 bResetAccumulator = false;

			//ImGui::Text("Render time: %.3f ms", Profiler::GetTimerResult(GlobalProfilingScope_RenderTime).lastElapsed * 1000.0f);

			ImGui::Text("Resolution: %ux%u", Inst->RenderWidth, Inst->RenderHeight);
			ImGui::Text("Accumulated frames: %u", Inst->AccumulatedFrameCount);
			ImGui::Text("Total Energy received: %.3f", Inst->AvgEnergyAccumulator / static_cast<f64>(Inst->AccumulatedFrameCount) * 1000.0f);

			// Debug category
			if (ImGui::CollapsingHeader("Debug"))
			{
				ImGui::Indent(10.0f);

				// Render data visualization
				ImGui::Text("Render data visualization mode");
				if (ImGui::BeginCombo("##Render data visualization mode", RenderDataVisualizationLabels[Inst->Settings.RenderVisualization], ImGuiComboFlags_None))
				{
					for (u32 i = 0; i < RenderVisualization_Count; ++i)
					{
						b8 bSelected = i == Inst->Settings.RenderVisualization;

						if (ImGui::Selectable(RenderDataVisualizationLabels[i], bSelected))
						{
							Inst->Settings.RenderVisualization = (RenderVisualization)i;
							bResetAccumulator = true;
						}

						if (bSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::Unindent(10.0f);
			}

			// Render Settings
			if (ImGui::CollapsingHeader("Settings"))
			{
				ImGui::Indent(10.0f);

				if (ImGui::SliderInt("Max Ray Recursion Depth: %u", reinterpret_cast<i32*>(&Inst->Settings.RayMaxRecursionDepth), 0, 8)) bResetAccumulator = true;

				// Enable/disable cosine weighted diffuse reflections, russian roulette
				if (ImGui::Checkbox("Cosine weighted diffuse", &Inst->Settings.bCosineWeightedDiffuseReflection)) bResetAccumulator = true;
				if (ImGui::Checkbox("Russian roulette", &Inst->Settings.bRussianRoulette)) bResetAccumulator = true;

				// HDR environment Intensity
				if (ImGui::DragFloat("HDR env Intensity", &Inst->Settings.HdrEnvIntensity, 0.001f)) bResetAccumulator = true;

				// Post-process Settings, constract, brightness, saturation, sRGB
				ImGui::SetNextItemOpen(true, ImGuiCond_Once);
				if (ImGui::CollapsingHeader("Post-processing"))
				{
					ImGui::Indent(10.0f);

					if (ImGui::SliderFloat("Max white", &Inst->Settings.Postfx.MaxWhite, 0.0f, 100.0f)) bResetAccumulator = true;
					if (ImGui::SliderFloat("Exposure", &Inst->Settings.Postfx.Exposure, 0.0f, 100.0f)) bResetAccumulator = true;
					if (ImGui::SliderFloat("Contrast", &Inst->Settings.Postfx.Contrast, 0.0f, 3.0f)) bResetAccumulator = true;
					if (ImGui::SliderFloat("Brightness", &Inst->Settings.Postfx.Brightness, -1.0f, 1.0f)) bResetAccumulator = true;
					if (ImGui::SliderFloat("Saturation", &Inst->Settings.Postfx.Saturation, 0.0f, 10.0f)) bResetAccumulator = true;
					if (ImGui::Checkbox("Linear to SRGB", &Inst->Settings.Postfx.bLinearToSRGB)) bResetAccumulator = true;

					ImGui::Unindent(10.0f);
				}

				ImGui::Unindent(10.0f);
			}

			if (bResetAccumulator)
				ResetAccumulation();
		}
		ImGui::End();
	}

	RenderTextureHandle CreateTexture(u32 Width, u32 Height, f32* PtrPixelData)
	{
		Texture Texture = {};
		Texture.Width = Width;
		Texture.Height = Height;
		Texture.PtrPixelData = ARENA_ALLOC_ARRAY(&Inst->Arena, f32, Width * Height * 4);
		memcpy(Texture.PtrPixelData, PtrPixelData, Width * Height * sizeof(float) * 4);

		RenderTextureHandle textureHandle = Inst->TextureSlotmap.Add(std::move(Texture));
		return textureHandle;
	}

	RenderMeshHandle CreateMesh(Vertex* Vertices, u32 VertexCount, u32* Indices, u32 IndexCount)
	{
		BVH::BuildOptions buildOptions = {};

		BVH meshBVH = {};
		meshBVH.Build(&Inst->Arena, Vertices, VertexCount, Indices, IndexCount, buildOptions);

		RenderMeshHandle meshHandle = Inst->BvhSlotmap.Add(std::move(meshBVH));
		return meshHandle;
	}

	void SubmitMesh(RenderMeshHandle RMeshHandle, const glm::mat4& Transform, const Material& Mat)
	{
		const BVH* BVH = Inst->BvhSlotmap.Find(RMeshHandle);
		if (!BVH)
			return;

		ASSERT(Inst->BVHInstanceAt < Inst->BVHInstanceCount);

		BVHInstance* Instance = &Inst->BVHInstances[Inst->BVHInstanceAt];
		Instance->SetBVH(BVH);
		Instance->SetTransform(Transform);

		Material* Material = &Inst->BVHMaterials[Inst->BVHInstanceAt];
		*Material = Mat;

		Inst->BVHInstanceAt++;
	}

}

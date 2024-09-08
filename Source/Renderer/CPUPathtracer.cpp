#include "pch.h"
#include "cpupathtracer.h"
#include "renderer_common.h"
#include "raytracing_utils.h"
#include "dx12/dx12_backend.h"

#include "acceleration_structure/bvh.h"
#include "acceleration_structure/bvh_instance.h"
#include "acceleration_structure/tlas.h"

#include "core/camera/camera_controller.h"
#include "core/logger.h"
#include "core/random.h"
#include "core/threadpool.h"

#include "resource_slotmap.h"

#include "imgui/imgui.h"

using namespace renderer;

namespace cpupathtracer
{

	struct instance_t
	{
		memory_arena_t* arena;
		u8* arena_marker_frame;

		u32* pixel_buffer;
		glm::vec4* pixel_accumulator;
		f64 avg_energy_accumulator;
		u32 accumulated_frame_count;

		threadpool_t render_threadpool;
	} static *inst;

	static void reset_accumulators()
	{
		// clear the accumulator, reset accumulated frame count
		memset(inst->pixel_accumulator, 0, g_renderer->render_width * g_renderer->render_height * sizeof(inst->pixel_accumulator[0]));
		inst->avg_energy_accumulator = 0.0;
		inst->accumulated_frame_count = 0;
	}

	static glm::vec3 sample_hdr_env(const glm::vec3& dir)
	{
		glm::vec2 uv = rt_util::direction_to_equirect_uv(dir);
		uv.y = glm::abs(uv.y - 1.0f);

		glm::uvec2 texel_pos = uv * glm::vec2(g_renderer->scene_hdr_env_texture->width, g_renderer->scene_hdr_env_texture->height);

		glm::vec4 hdr_env_color = {};
		hdr_env_color.r = g_renderer->scene_hdr_env_texture->ptr_pixel_data[texel_pos.y * g_renderer->scene_hdr_env_texture->width * 4 + texel_pos.x * 4];
		hdr_env_color.g = g_renderer->scene_hdr_env_texture->ptr_pixel_data[texel_pos.y * g_renderer->scene_hdr_env_texture->width * 4 + texel_pos.x * 4 + 1];
		hdr_env_color.b = g_renderer->scene_hdr_env_texture->ptr_pixel_data[texel_pos.y * g_renderer->scene_hdr_env_texture->width * 4 + texel_pos.x * 4 + 2];
		hdr_env_color.a = g_renderer->scene_hdr_env_texture->ptr_pixel_data[texel_pos.y * g_renderer->scene_hdr_env_texture->width * 4 + texel_pos.x * 4 + 3];

		return hdr_env_color.xyz;
	}

	static glm::vec3 apply_post_processing(const glm::vec3& color)
	{
		glm::vec3 final_color = color;

		if (g_renderer->settings.render_view_mode != RenderViewMode_None)
		{
			if (g_renderer->settings.render_view_mode == RenderViewMode_HitAlbedo ||
				g_renderer->settings.render_view_mode == RenderViewMode_HitEmissive ||
				g_renderer->settings.render_view_mode == RenderViewMode_HitAbsorption)
				return rt_util::linear_to_srgb(final_color);
			else
				return final_color;
		}

		// Apply exposure
		final_color *= g_renderer->settings.postfx.exposure;

		// contrast & brightness
		final_color = rt_util::apply_contrast_brightness(final_color, g_renderer->settings.postfx.contrast, g_renderer->settings.postfx.brightness);

		// saturation
		final_color = rt_util::apply_saturation(final_color, g_renderer->settings.postfx.saturation);

		// Apply simple tonemapper
		final_color = rt_util::tonemap_reinhard_white(final_color.xyz, g_renderer->settings.postfx.max_white);

		// Convert final color from linear to SRGB, if enabled, and if we do not use any render data visualization
		if (g_renderer->settings.postfx.linear_to_srgb)
			final_color.xyz = rt_util::linear_to_srgb(final_color.xyz);

		return final_color;
	}

	static glm::vec4 trace_path(ray_t& ray)
	{
		// TODO: Make memory arena statistics and visualizations using Dear ImPlot library
		// TODO: Custom string class, counted strings, use in Assertion.h, logger.h, EntryPoint.cpp for command line parsing
		// TODO: SafeTruncate for int and uint types, asserting if the value of a u64 is larger than the one to truncate to
		// TODO: Make any resolution work with the multi-threaded rendering dispatch
		// TODO: find a better epsilon for the Möller-Trumbore Triangle Intersection Algorithm
		// TODO: add Config.h which has a whole bunch of defines like which intersection algorithms to use and what not
		// REMEMBER: Set DXC to not use legacy struct padding
		// TOOD: DX12 Agility SDK & Enhanced Barriers
		// TODO: Profiling
		// TODO: application window for profiler, Timer avg/min/max
		// TODO: Profiling for tlas_t builds
		// TODO: Next event estimation
		// TODO: Stop moving mouse back to center when window loses focus
		// TODO: Profile BVH build times for the BLAS and also the tlas_t
		
		// TODO: Setting for accumulation should be a render setting, with defaults for each render visualization
		// TODO: BVH Refitting and Rebuilding (https://jacco.ompf2.com/2022/04/26/how-to-build-a-bvh-part-4-animation/)
		// TODO: Display BVH build data like max depth, total number of vertices/triangles, etc. in the mesh assets once I have menus for that
		// TODO: Separate BVH Builder and BVH's, which should just contain the actual data after a finished BVH build
		// TODO: Assets, loading and storing BVH's
		// TODO: DOF (https://youtu.be/Qz0KTGYJtUk)
		// TODO: GPU Pathtracer
			// TODO: GPU wavefront path tracer and path tracer inline raytracing for comparisons of performance, etc.
			// NOTE: Need to figure out how the application-renderer interface should look like, the application should not know and/or care
			//		 whether the frame was rendered using the GPU or CPU pathtracer
		// FIX: Sometimes the colors just get flat and not even resetting the accumulator fixes it??
		// TODO: Transmittance and density instead of absorption?
		// TODO: Total Energy received, accumulated over time so it slowly stabilizes, for comparisons
			// NOTE: Calculate average by using previous frame value * numAccumulated - 1 and current frame just times 1
			// FIX: Why does the amount of average Energy received change when we toggle inst->Settings.linearToSRGB?
		// TODO: Window resizing and resolution resizing
		// TODO: Tooltips for render data visualization modes to know what they do, useful for e.g. ray recursion depth or RR kill depth
		// TODO: Scene hierarchy
			// TODO: Spawn new objects from ImGui
		// TODO: ImGuizmo to transform objects
		// TODO: ray/path visualization mode
		// TODO: Unit tests for rt_util functions like UniformHemisphereSampling (testing the resulting dir for length 1 for example)
		// TODO: Physically-based rendering
		// TODO: BRDF importance sampling
		// TODO: Denoising
		// TODO: ReSTIR

		glm::vec3 throughput(1.0f);
		glm::vec3 energy(0.0f);

		u32 ray_depth = 0;
		b8 specular_ray = false;
		b8 survived_rr = true;

		while (ray_depth <= g_renderer->settings.ray_max_recursion)
		{
			hit_result_t hit_result = {};
			g_renderer->scene_tlas.trace_ray(ray, hit_result);

			// render visualizations that are not depending on valid geometry data
			if (g_renderer->settings.render_view_mode != RenderViewMode_None)
			{
				b8 stop_tracing = false;

				switch (g_renderer->settings.render_view_mode)
				{
					case RenderViewMode_AccelerationStructureDepth: energy = glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), ray.bvh_depth / 50.0f); stop_tracing = true; break;
				}

				if (stop_tracing)
					break;
			}

			// add sky color to Energy if we have not Hit an object, and terminate path
			if (!hit_result.has_hit_geometry())
			{
				energy += g_renderer->settings.hdr_env_intensity * sample_hdr_env(ray.dir) * throughput;
				break;
			}

			material_t hit_material = g_renderer->bvh_instances_materials[hit_result.instance_idx];

			// render visualizations that are depending on valid geometry data
			if (g_renderer->settings.render_view_mode != RenderViewMode_None)
			{
				b8 stop_tracing = false;

				switch (g_renderer->settings.render_view_mode)
				{
				case RenderViewMode_HitAlbedo:		  energy = hit_material.albedo; stop_tracing = true; break;
				case RenderViewMode_HitNormal:		  energy = glm::abs(hit_result.normal); stop_tracing = true; break;
				case RenderViewMode_HitBarycentrics: energy = hit_result.bary; stop_tracing = true; break;
				case RenderViewMode_HitSpecRefract:  energy = glm::vec3(hit_material.specular, hit_material.refractivity, 0.0f); stop_tracing = true; break;
				case RenderViewMode_HitAbsorption:	  energy = glm::vec3(hit_material.absorption); stop_tracing = true; break;
				case RenderViewMode_HitEmissive:	  energy = glm::vec3(hit_material.emissive_color * hit_material.emissive_intensity * static_cast<f32>(hit_material.emissive)); stop_tracing = true; break;
				case RenderViewMode_Depth:			  energy = glm::vec3(hit_result.t * 0.01f); stop_tracing = true; break;
				}

				if (stop_tracing)
					break;
			}

			// If the surface is emissive_color, we simply add its color to the Energy output and stop tracing
			if (hit_material.emissive)
			{
				energy += hit_material.emissive_color * hit_material.emissive_intensity * throughput;
				break;
			}

			// Russian roulette - Stop tracing the path with a probability that is based on the material albedo
			// Since the Throughput is gonna be dependent on the albedo, dark albedos carry less Energy to the eye,
			// so we can eliminate those paths with a higher probability and safe ourselves the time continuing tracing low impact paths.
			// Need to adjust the Throughput if the path survives since that path should carry a higher weight based on its survival probability.
			if (g_renderer->settings.russian_roulette)
			{
				f32 survival_prob_rr = rt_util::survival_probability_rr(hit_material.albedo);
				if (survival_prob_rr < random::rand_float())
				{
					survived_rr = false;
					break;
				}
				else
				{
					throughput *= 1.0f / survival_prob_rr;
				}
			}

			// Get a random 0..1 range rand_float to determine which path to take
			f32 r = random::rand_float();

			// specular path
			if (r < hit_material.specular)
			{
				glm::vec3 spec_dir = rt_util::reflect(ray.dir, hit_result.normal);
				ray = make_ray(hit_result.pos + spec_dir * RAY_NUDGE_MODIFIER, spec_dir);

				throughput *= hit_material.albedo;
				specular_ray = true;
			}
			// Refraction path
			else if (r < hit_material.specular + hit_material.refractivity)
			{
				glm::vec3 N = hit_result.normal;

				f32 cosi = glm::clamp(glm::dot(N, ray.dir), -1.0f, 1.0f);
				f32 etai = 1.0f, etat = hit_material.ior;

				f32 Fr = 1.0f;
				b8 inside = true;

				// Figure out if we are bInside or outside of the object we've Hit
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

				f32 eta = etai / etat;
				f32 k = 1.0f - eta * eta * (1.0f - cosi * cosi);

				// Follow refraction or specular path
				if (k >= 0.0f)
				{
					glm::vec3 refract_dir = rt_util::refract(ray.dir, N, eta, cosi, k);

					f32 cos_in = glm::dot(ray.dir, hit_result.normal);
					f32 cos_out = glm::dot(refract_dir, hit_result.normal);

					Fr = rt_util::fresnel(cos_in, cos_out, etai, etat);

					if (random::rand_float() > Fr)
					{
						throughput *= hit_material.albedo;

						if (inside)
						{
							glm::vec3 absorption = glm::vec3(
								glm::exp(-hit_material.absorption.x * ray.t),
								glm::exp(-hit_material.absorption.y * ray.t),
								glm::exp(-hit_material.absorption.z * ray.t)
							);
							throughput *= absorption;
						}

						ray = make_ray(hit_result.pos + refract_dir * RAY_NUDGE_MODIFIER, refract_dir);
						specular_ray = true;
					}
					else
					{
						glm::vec3 spec_dir = rt_util::reflect(ray.dir, hit_result.normal);
						ray = make_ray(hit_result.pos + spec_dir * RAY_NUDGE_MODIFIER, spec_dir);

						throughput *= hit_material.albedo;
						specular_ray = true;
					}
				}
			}
			// Diffuse path
			else
			{
				glm::vec3 diffuse_brdf = hit_material.albedo * INV_PI;
				glm::vec3 diffuse_dir(0.0f);

				f32 NdotR = 0.0f;
				f32 hemi_pdf = 0.0f;

				// Cosine-weighted diffuse reflection
				if (g_renderer->settings.cosine_weighted_diffuse)
				{
					diffuse_dir = rt_util::cosine_weighted_hemisphere_sample(hit_result.normal);
					NdotR = glm::dot(diffuse_dir, hit_result.normal);
					hemi_pdf = NdotR * INV_PI;
				}
				// Uniform hemisphere diffuse reflection
				else
				{
					diffuse_dir = rt_util::uniform_hemisphere_sample(hit_result.normal);
					NdotR = glm::dot(diffuse_dir, hit_result.normal);
					hemi_pdf = INV_TWO_PI;
				}

				ray = make_ray(hit_result.pos + diffuse_dir * RAY_NUDGE_MODIFIER, diffuse_dir);
				throughput *= (NdotR * diffuse_brdf) / hemi_pdf;
				specular_ray = false;
			}

			ray_depth++;
		}

		// handle any render data visualization that needs to trace the full path first
		if (g_renderer->settings.render_view_mode != RenderViewMode_None)
		{
			switch (g_renderer->settings.render_view_mode)
			{
			case RenderViewMode_RayRecursionDepth:
			{
				energy = glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), ray_depth / static_cast<f32>(g_renderer->settings.ray_max_recursion));
			} break;
			case RenderViewMode_RussianRouletteKillDepth:
			{
				f32 Weight = glm::clamp((ray_depth / static_cast<f32>(g_renderer->settings.ray_max_recursion)) - static_cast<f32>(survived_rr), 0.0f, 1.0f);
				energy = glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), Weight);
			} break;
			}
		}

		return glm::vec4(energy, 1.0f);
	}

	void init(memory_arena_t* arena)
	{
		LOG_INFO("CPU Pathtracer", "Init");
		
		inst = ARENA_ALLOC_STRUCT_ZERO(arena, instance_t);
		inst->arena = arena;

		inst->pixel_buffer = ARENA_ALLOC_ARRAY_ZERO(inst->arena, u32, g_renderer->render_width * g_renderer->render_height);
		inst->pixel_accumulator = ARENA_ALLOC_ARRAY_ZERO(inst->arena, glm::vec4, g_renderer->render_width * g_renderer->render_height);

		inst->render_threadpool.init(inst->arena);
	}

	void exit()
	{
		LOG_INFO("CPU Pathtracer", "Exit");
		
		inst->render_threadpool.destroy();
	}

	void begin_frame()
	{
		if (g_renderer->settings.render_view_mode != RenderViewMode_None)
			reset_accumulators();

		inst->arena_marker_frame = inst->arena->ptr_at;
	}

	void end_frame()
	{
		dx12_backend::copy_to_back_buffer(reinterpret_cast<u8*>(inst->pixel_buffer));
		ARENA_FREE(inst->arena, inst->arena_marker_frame);
	}

	void begin_scene(const camera_t& scene_camera)
	{
		if (g_renderer->scene_camera.view_mat != scene_camera.view_mat)
			reset_accumulators();
	}

	void render()
	{
		inst->accumulated_frame_count++;

		// build the current scene's tlas_t
		g_renderer->scene_tlas.build(&g_renderer->arena, g_renderer->bvh_instances, g_renderer->bvh_instances_at);

		f32 aspect = g_renderer->render_width / static_cast<f32>(g_renderer->render_height);
		f32 tan_fov = glm::tan(glm::radians(g_renderer->scene_camera.vfov_deg) / 2.0f);
		
		u32 pixel_count = g_renderer->render_width * g_renderer->render_height;
		f64 inv_pixel_count = 1.0f / static_cast<f32>(pixel_count);

		glm::uvec2 render_dispatch_dim = { 16, 16 };
		ASSERT(g_renderer->render_width % render_dispatch_dim.x == 0);
		ASSERT(g_renderer->render_height % render_dispatch_dim.y == 0);

		auto trace_path_job = [&render_dispatch_dim, &aspect, &tan_fov, &inv_pixel_count](threadpool_t::job_dispatch_args_t dispatch_args) {
			const u32 dispatch_x = (dispatch_args.job_index * render_dispatch_dim.x) % g_renderer->render_width;
			const u32 dispatch_y = ((dispatch_args.job_index * render_dispatch_dim.x) / g_renderer->render_width) * render_dispatch_dim.y;

			for (u32 y = dispatch_y; y < dispatch_y + render_dispatch_dim.y; ++y)
			{
				for (u32 x = dispatch_x; x < dispatch_x + render_dispatch_dim.x; ++x)
				{
					// Construct primary ray from camera through the current pixel
					ray_t ray = rt_util::construct_camera_ray(g_renderer->scene_camera, x, y, tan_fov,
						aspect, g_renderer->inv_render_width, g_renderer->inv_render_height);

					// Start tracing path through pixel
					glm::vec4 path_color = trace_path(ray);

					// update accumulator
					u32 pixel_pos = y * g_renderer->render_width + x;
					inst->pixel_accumulator[pixel_pos] += path_color;
					inst->avg_energy_accumulator += static_cast<f64>(path_color.x + path_color.y + path_color.z) * inv_pixel_count;

					// Determine final color for pixel
					glm::vec4 final_color = inst->pixel_accumulator[pixel_pos] /
						static_cast<f32>(inst->accumulated_frame_count);

					// Apply post-processing stack to final color
					final_color.xyz = apply_post_processing(final_color.xyz);

					// Write final color
					inst->pixel_buffer[pixel_pos] = rt_util::vec4_to_u32(final_color);
				}
			}
		};

		u32 job_count = pixel_count / (render_dispatch_dim.x * render_dispatch_dim.y);

		inst->render_threadpool.dispatch(job_count, 16, trace_path_job);
		inst->render_threadpool.wait_all();
	}

	void end_scene()
	{
	}

	void render_ui(b8 reset_accumulation)
	{
		ImGui::Text("Accumulated frames: %u", inst->accumulated_frame_count);
		ImGui::Text("Total energy received: %.3f", inst->avg_energy_accumulator / static_cast<f64>(inst->accumulated_frame_count) * 1000.0f);

		if (reset_accumulation)
			reset_accumulators();
	}

}

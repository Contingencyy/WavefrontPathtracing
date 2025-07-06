#include "pch.h"
#include "renderer/gpu_profiler.h"
#include "renderer/renderer_common.h"

#include "d3d12/d3d12_common.h"
#include "d3d12/d3d12_backend.h"
#include "d3d12/d3d12_query.h"

#include "imgui/imgui.h"

namespace renderer
{

	static void do_timestamp_query(frame_context_t& frame_ctx, ID3D12GraphicsCommandList10* command_list, GPU_PROFILE_SCOPE scope, bool begin)
	{
		ASSERT(scope < GPU_PROFILE_SCOPE_COUNT);
		ASSERT(frame_ctx.gpu_timer_queries_at < d3d12::TIMESTAMP_QUERIES_DEFAULT_CAPACITY);

		gpu_timer_query_t& query = frame_ctx.gpu_timer_queries[frame_ctx.gpu_timer_queries_at++];
		query.scope = scope;
		query.type = begin ? GPU_TIMESTAMP_QUERY_TYPE_BEGIN : GPU_TIMESTAMP_QUERY_TYPE_END;
		query.query_idx = begin ? d3d12::begin_query_timestamp(command_list) : d3d12::end_query_timestamp(command_list);

		switch (command_list->GetType())
		{
		case D3D12_COMMAND_LIST_TYPE_DIRECT: query.d3d_command_queue = d3d12::g_d3d->command_queue_direct; break;
		case D3D12_COMMAND_LIST_TYPE_COPY: query.d3d_command_queue = d3d12::g_d3d->upload_ctx.command_queue_copy; break;
		default: FATAL_ERROR("Renderer", "Tried to begin a timestamp query on an unsupported command list type"); break;
		}
	}
	
	static void render_profile_scope_ui(GPU_PROFILE_SCOPE scope, double timestamp_freq)
	{
		ASSERT(scope < GPU_PROFILE_SCOPE_COUNT);
		const gpu_profile_scope_result_t& scope_result = g_renderer->gpu_profile_scope_results[scope];

		if (scope_result.sample_count == 0)
			return;

		double total = ((double)scope_result.total / timestamp_freq) * 1000.0;
		double avg = (total / (double)scope_result.sample_count);
		double min = ((double)scope_result.min / timestamp_freq) * 1000.0;
		double max = ((double)scope_result.max / timestamp_freq) * 1000.0;

		ImGui::Text("%s: %.3f ms", gpu_profile_scope_labels[scope], total);
		if (ImGui::BeginItemTooltip())
		{
			if (scope_result.sample_count == 1)
			{
				ImGui::Text("Samples: %u", scope_result.sample_count);
				ImGui::Text("Total: %.3f ms", total);
			}
			else
			{
				ImGui::Text("Samples: %u", scope_result.sample_count);
				ImGui::Text("Total: %.3f ms", total);
				ImGui::Text("Average: %.3f ms", avg);
				ImGui::Text("Min: %.3f ms", min);
				ImGui::Text("Max: %.3f ms", max);
			}

			ImGui::EndTooltip();
		}
	}

	void gpu_profiler_begin_scope(frame_context_t& frame_ctx, ID3D12GraphicsCommandList10* command_list, GPU_PROFILE_SCOPE scope)
	{
		do_timestamp_query(frame_ctx, command_list, scope, true);
	}

	void gpu_profiler_end_scope(frame_context_t& frame_ctx, ID3D12GraphicsCommandList10* command_list, GPU_PROFILE_SCOPE scope)
	{
		do_timestamp_query(frame_ctx, command_list, scope, false);
	}

	void gpu_profiler_readback_timers()
	{
		const d3d12::frame_context_t d3d_frame_ctx = d3d12::get_frame_context();
		frame_context_t& frame_ctx = get_frame_context();

		if (frame_ctx.gpu_timer_queries_at > 0)
		{
			ID3D12Resource* readback_buffer = d3d_frame_ctx.readback_buffer_timestamps;
			uint64_t* timestamp_values = (uint64_t*)d3d12::map_resource(readback_buffer);

			for (uint32_t i = 0; i < frame_ctx.gpu_timer_queries_at; ++i)
			{
				gpu_timer_query_t& query = frame_ctx.gpu_timer_queries[i];
				query.timestamp = timestamp_values[query.query_idx];
			}

			d3d12::unmap_resource(readback_buffer);
		}
	}

	void gpu_profiler_parse_scope_results()
	{
		frame_context_t& frame_ctx = get_frame_context();
		uint32_t readback_begin_result_indices[GPU_PROFILE_SCOPE_COUNT];

		for (uint32_t i = 0; i < GPU_PROFILE_SCOPE_COUNT; ++i)
		{
			readback_begin_result_indices[i] = UINT32_MAX;
			
			g_renderer->gpu_profile_scope_results[i].sample_count = 0u;
			g_renderer->gpu_profile_scope_results[i].total = 0ull;
			g_renderer->gpu_profile_scope_results[i].min = UINT64_MAX;
			g_renderer->gpu_profile_scope_results[i].max = 0ull;
		}
		
		for (uint32_t i = 0; i < frame_ctx.gpu_timer_queries_at; ++i)
		{
			const gpu_timer_query_t& readback = frame_ctx.gpu_timer_queries[i];
			gpu_profile_scope_result_t& scope_result = g_renderer->gpu_profile_scope_results[readback.scope];
			ASSERT(readback.scope < GPU_PROFILE_SCOPE_COUNT);

			if (readback_begin_result_indices[readback.scope] == UINT32_MAX)
			{
				ASSERT_MSG(readback.type == GPU_TIMESTAMP_QUERY_TYPE_BEGIN, "GPU profile scope %s was ended before being started", gpu_profile_scope_labels[readback.scope]);
				readback_begin_result_indices[readback.scope] = i;
			}
			else
			{
				ASSERT_MSG(readback.type == GPU_TIMESTAMP_QUERY_TYPE_END, "GPU profile scope %s was started twice in a row before being ended first", gpu_profile_scope_labels[readback.scope]);
				const gpu_timer_query_t& readback_begin = frame_ctx.gpu_timer_queries[readback_begin_result_indices[readback.scope]];

				uint64_t delta = readback.timestamp - readback_begin.timestamp;
				scope_result.sample_count++;
				scope_result.total += delta;
				scope_result.min = MIN(scope_result.min, delta);
				scope_result.max = MAX(scope_result.max, delta);

				readback_begin_result_indices[readback.scope] = UINT32_MAX;
			}
		}

		for (uint32_t i = 0; i < GPU_PROFILE_SCOPE_COUNT; ++i)
		{
			ASSERT_MSG(readback_begin_result_indices[i] == UINT32_MAX, "GPU profile scope %s has outstanding samples", gpu_profile_scope_labels[i]);
		}
	}

	void gpu_profiler_render_ui()
	{
		if (ImGui::CollapsingHeader("GPU Timers"))
		{
			// TODO: Get the correct timestamp frequency for each command queue, maybe store it inside the gpu timers
			double timestamp_freq = (double)d3d12::get_timestamp_frequency();

			if (g_renderer->gpu_profile_scope_results[GPU_PROFILE_SCOPE_TOTAL_GPU_TIME].sample_count > 0)
			{
				const gpu_profile_scope_result_t& scope_result = g_renderer->gpu_profile_scope_results[GPU_PROFILE_SCOPE_TOTAL_GPU_TIME];
				render_profile_scope_ui(GPU_PROFILE_SCOPE_TOTAL_GPU_TIME, timestamp_freq);
			}

			for (uint32_t i = 0; i < GPU_PROFILE_SCOPE_COUNT; ++i)
			{
				if (i == GPU_PROFILE_SCOPE_TOTAL_GPU_TIME)
					continue;

				render_profile_scope_ui((GPU_PROFILE_SCOPE)i, timestamp_freq);
			}
		}
	}
}

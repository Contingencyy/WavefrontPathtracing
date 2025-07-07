#include "pch.h"
#include "renderer/gpu_profiler.h"
#include "renderer/renderer_common.h"

#include "d3d12/d3d12_common.h"
#include "d3d12/d3d12_backend.h"
#include "d3d12/d3d12_query.h"

#include "imgui/imgui.h"
#include "implot/implot.h"

namespace renderer
{

	static void do_timestamp_query(frame_context_t& frame_ctx, ID3D12GraphicsCommandList10* command_list, GPU_PROFILE_SCOPE scope, bool begin)
	{
		ASSERT(scope < GPU_PROFILE_SCOPE_COUNT);
		ASSERT(frame_ctx.gpu_timer_queries_at < d3d12::TIMESTAMP_QUERIES_DEFAULT_CAPACITY);

		gpu_timer_query_t& query = frame_ctx.gpu_timer_queries[frame_ctx.gpu_timer_queries_at++];
		query.frame_index = g_renderer->frame_index;
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
		uint32_t data_offset = g_renderer->gpu_profiler.history_write_offset * GPU_PROFILE_SCOPE_COUNT;

		// TODO: Get the correct timestamp frequency for each command queue, maybe store it inside the gpu timers
		double timestamp_freq = (double)d3d12::get_timestamp_frequency();

		for (uint32_t i = 0; i < GPU_PROFILE_SCOPE_COUNT; ++i)
		{
			readback_begin_result_indices[i] = UINT32_MAX;

			gpu_profile_scope_result_t& scope_result = g_renderer->gpu_profiler.profile_scope_results[data_offset + i];
			scope_result.frame_index = 0;
			scope_result.sample_count = 0u;
			scope_result.total = 0.0;
			scope_result.avg = 0.0;
			scope_result.min = DBL_MAX;
			scope_result.max = 0.0;
		}
		
		for (uint32_t i = 0; i < frame_ctx.gpu_timer_queries_at; ++i)
		{
			const gpu_timer_query_t& readback = frame_ctx.gpu_timer_queries[i];
			gpu_profile_scope_result_t& scope_result = g_renderer->gpu_profiler.profile_scope_results[data_offset + readback.scope];
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
				scope_result.frame_index = (double)readback.frame_index;
				scope_result.sample_count++;
				scope_result.total += ((double)delta / timestamp_freq) * 1000.0;
				scope_result.min = MIN(scope_result.min, ((double)delta / timestamp_freq) * 1000.0);
				scope_result.max = MAX(scope_result.max, ((double)delta / timestamp_freq) * 1000.0);

				readback_begin_result_indices[readback.scope] = UINT32_MAX;
			}
		}

		for (uint32_t i = 0; i < GPU_PROFILE_SCOPE_COUNT; ++i)
		{
			ASSERT_MSG(readback_begin_result_indices[i] == UINT32_MAX, "GPU profile scope %s has outstanding samples", gpu_profile_scope_labels[i]);

			gpu_profile_scope_result_t& scope_result = g_renderer->gpu_profiler.profile_scope_results[data_offset + i];
			scope_result.avg = (scope_result.total / (double)scope_result.sample_count);
		}
	}

	void gpu_profiler_render_ui()
	{
		if (ImGui::Begin("GPU Profiler"))
		{
			ImPlot::SetNextAxisToFit(ImAxis_Y1);
			if (ImPlot::BeginPlot("##gpu_timer_graph", ImVec2(-1, 0), ImPlotFlags_Crosshairs | ImPlotFlags_NoMouseText))
			{
				double axis_min = MAX((int64_t)g_renderer->frame_index - (int32_t)d3d12::g_d3d->swapchain.back_buffer_count - (int32_t)GPU_PROFILER_MAX_HISTORY, 0.0);
				double axis_max = (int64_t)g_renderer->frame_index - (int32_t)d3d12::g_d3d->swapchain.back_buffer_count;

				ImPlot::SetupAxis(ImAxis_X1, "Frame Index", ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit | ImPlotAxisFlags_NoGridLines);
				ImPlot::SetupAxisFormat(ImAxis_X1, "%.0f");
				ImPlot::SetupAxisLimits(ImAxis_X1, axis_min, axis_max, ImPlotCond_Always);

				ImPlot::SetupAxis(ImAxis_Y1, "Milliseconds", ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoGridLines);
				ImPlot::SetupAxisFormat(ImAxis_Y1, "%.3f ms");

				ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
				int32_t count = GPU_PROFILER_MAX_HISTORY;
				int32_t data_offset = (int32_t)g_renderer->gpu_profiler.history_read_offset;
				int32_t data_stride = GPU_PROFILE_SCOPE_COUNT * sizeof(gpu_profile_scope_result_t);

				for (uint32_t i = 0; i < GPU_PROFILE_SCOPE_COUNT; ++i)
				{
					const gpu_profile_scope_result_t& scope_result = g_renderer->gpu_profiler.profile_scope_results[i];

					ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.5f);
					ImPlot::PlotShaded(gpu_profile_scope_labels[i], &scope_result.frame_index, &scope_result.total, count, 0.0, 0, data_offset, data_stride);
					ImPlot::PlotLine(gpu_profile_scope_labels[i], &scope_result.frame_index, &scope_result.total, count, 0, data_offset, data_stride);
					ImPlot::PopStyleVar();
				}

				ImPlot::EndPlot();
			}

			ImGui::End();
		}
	}
}

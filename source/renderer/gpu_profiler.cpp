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

	void gpu_profiler_init()
	{
		gpu_profiler_t& profiler = g_renderer->gpu_profiler;
		profiler.history_write_offset = 0;
		profiler.history_read_offset = 1;
		profiler.autofit_timers = true;
	}

	void gpu_profiler_exit()
	{
	}

	void gpu_profiler_begin_frame()
	{
		// Read back the GPU timestamp queries from the buffer and parse the final gpu profile scopes
		gpu_profiler_readback_timers();
		gpu_profiler_parse_scope_results();
	}

	void gpu_profiler_end_frame()
	{
		gpu_profiler_t& profiler = g_renderer->gpu_profiler;
		
		// We don't have any profiling data to write back the first time each back buffer is used, so skip incrementing the history offsets
		if (g_renderer->frame_index >= d3d12::g_d3d->swapchain.back_buffer_count)
		{
			profiler.history_write_offset = (profiler.history_write_offset + 1) % GPU_PROFILER_MAX_HISTORY;
			profiler.history_read_offset = (profiler.history_write_offset + 1) % GPU_PROFILER_MAX_HISTORY;
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
		gpu_profiler_t& profiler = g_renderer->gpu_profiler;
		frame_context_t& frame_ctx = get_frame_context();
		uint32_t readback_begin_result_indices[GPU_PROFILE_SCOPE_COUNT];

		// TODO: Get the correct timestamp frequency for each command queue, maybe store it inside the gpu timers
		double timestamp_freq = (double)d3d12::get_timestamp_frequency();

		for (uint32_t scope = 0; scope < GPU_PROFILE_SCOPE_COUNT; ++scope)
		{
			readback_begin_result_indices[scope] = UINT32_MAX;

			gpu_profile_scope_result_t& scope_result = profiler.profile_scope_history[profiler.history_write_offset][scope];
			scope_result.frame_index = 0.0;
			scope_result.sample_count = 0u;
			scope_result.total = 0.0;
			scope_result.avg = 0.0;
			scope_result.min = DBL_MAX;
			scope_result.max = 0.0;
		}
		
		for (uint32_t query_idx = 0; query_idx < frame_ctx.gpu_timer_queries_at; ++query_idx)
		{
			const gpu_timer_query_t& readback = frame_ctx.gpu_timer_queries[query_idx];
			gpu_profile_scope_result_t& scope_result = profiler.profile_scope_history[profiler.history_write_offset][readback.scope];
			ASSERT(readback.scope < GPU_PROFILE_SCOPE_COUNT);

			if (readback_begin_result_indices[readback.scope] == UINT32_MAX)
			{
				ASSERT_MSG(readback.type == GPU_TIMESTAMP_QUERY_TYPE_BEGIN, "GPU profile scope %s was ended before being started", gpu_profile_scope_labels[readback.scope]);
				readback_begin_result_indices[readback.scope] = query_idx;
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

		for (uint32_t scope = 0; scope < GPU_PROFILE_SCOPE_COUNT; ++scope)
		{
			ASSERT_MSG(readback_begin_result_indices[scope] == UINT32_MAX, "GPU profile scope %s has outstanding samples", gpu_profile_scope_labels[scope]);
			gpu_profile_scope_result_t& scope_result = profiler.profile_scope_history[profiler.history_write_offset][scope];

			if (scope_result.sample_count == 0)
				continue;

			scope_result.avg = (scope_result.total / (double)scope_result.sample_count);
		}
	}

	void gpu_profiler_render_ui()
	{
		gpu_profiler_t& profiler = g_renderer->gpu_profiler;
		
		if (ImGui::Begin("GPU Profiler"))
		{
			ImGui::Checkbox("Autofit Timers", &profiler.autofit_timers);
			if (profiler.autofit_timers)
			{
				ImPlot::SetNextAxisToFit(ImAxis_Y1);
			}
			
			if (ImPlot::BeginPlot("Real-time GPU graph", ImVec2(-1, 0), ImPlotFlags_Crosshairs | ImPlotFlags_NoMouseText))
			{
				// Draw GPU timer plot
				ImPlot::SetupLegend(ImPlotLocation_NorthEast, ImPlotLegendFlags_Outside);

				double axis_min = (double)profiler.profile_scope_history[profiler.history_read_offset][GPU_PROFILE_SCOPE_TOTAL_GPU_TIME].frame_index;
				double axis_max = (double)profiler.profile_scope_history[profiler.history_write_offset][GPU_PROFILE_SCOPE_TOTAL_GPU_TIME].frame_index;

				ImPlot::SetupAxis(ImAxis_X1, "Frame Index", ImPlotAxisFlags_Foreground | ImPlotAxisFlags_RangeFit);
				ImPlot::SetupAxisFormat(ImAxis_X1, "%.0f");
				ImPlot::SetupAxisLimits(ImAxis_X1, axis_min, axis_max, ImPlotCond_Always);
				
				ImPlot::SetupAxis(ImAxis_Y1, "Milliseconds", ImPlotAxisFlags_Foreground | ImPlotAxisFlags_RangeFit);
				ImPlot::SetupAxisFormat(ImAxis_Y1, "%.3f ms");
				ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, 1000.0);
				
				ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
				int32_t count = GPU_PROFILER_MAX_HISTORY;
				int32_t data_offset = (int32_t)g_renderer->gpu_profiler.history_read_offset;
				int32_t data_stride = GPU_PROFILE_SCOPE_COUNT * sizeof(gpu_profile_scope_result_t);

				for (uint32_t scope = 0; scope < GPU_PROFILE_SCOPE_COUNT; ++scope)
				{
					const gpu_profile_scope_result_t& scope_result_start = profiler.profile_scope_history[0][scope];
					if (scope_result_start.sample_count == 0)
						continue;

					ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
					ImPlot::PlotShaded(gpu_profile_scope_labels[scope], &scope_result_start.frame_index, &scope_result_start.total, count, 0.0, 0, data_offset, data_stride);
					ImPlot::PlotLine(gpu_profile_scope_labels[scope], &scope_result_start.frame_index, &scope_result_start.total, count, 0, data_offset, data_stride);
					ImPlot::PopStyleVar();

					// Draw legend hover tooltip
					if (ImPlot::IsLegendEntryHovered(gpu_profile_scope_labels[scope]))
					{
						if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
						{
							profiler.profile_scope_graph_hidden[scope] = !profiler.profile_scope_graph_hidden[scope];
						}

						const gpu_profile_scope_result_t& scope_result = profiler.profile_scope_history[profiler.history_write_offset][scope];

						ImGui::BeginTooltip();

						ImGui::Text("Frame: %llu", (uint64_t)scope_result.frame_index);
						ImGui::Text("Samples: %u", scope_result.sample_count);
						ImGui::Text("Total: %.3f", scope_result.total);
						ImGui::Text("Avg: %.3f", scope_result.avg);
						ImGui::Text("Min: %.3f", scope_result.min);
						ImGui::Text("Max: %.3f", scope_result.max);

						ImGui::EndTooltip();
					}
				}

				// Draw nearest plot data point hover tooltip
				if (ImPlot::IsPlotHovered())
				{
					ImPlotPoint mouse_pos = ImPlot::GetPlotMousePos(ImAxis_X1, ImAxis_Y1);
					uint32_t nearest_history_frame_index = (uint32_t)glm::round(mouse_pos.x) % GPU_PROFILER_MAX_HISTORY;
					GPU_PROFILE_SCOPE nearest_history_scope = GPU_PROFILE_SCOPE_COUNT;

					double prev_delta = DBL_MAX, delta = 0.0;
					for (uint32_t scope = 0; scope < GPU_PROFILE_SCOPE_COUNT; ++scope)
					{
						if (profiler.profile_scope_graph_hidden[scope])
							continue;

						const gpu_profile_scope_result_t& history = profiler.profile_scope_history[nearest_history_frame_index][scope];
						if (history.sample_count == 0)
							continue;

						delta = fabs(mouse_pos.y - history.total);
						if (delta < prev_delta)
						{
							nearest_history_scope = (GPU_PROFILE_SCOPE)scope;
							prev_delta = delta;
						}
					}

					const gpu_profile_scope_result_t& nearest_history = profiler.profile_scope_history[nearest_history_frame_index][nearest_history_scope];

					ImPlot::PushPlotClipRect();
					ImGui::BeginTooltip();
					
					ImGui::Text("%s", gpu_profile_scope_labels[nearest_history_scope]);
					ImGui::Text("Frame: %llu", (uint64_t)nearest_history.frame_index);
					ImGui::Text("Frame: %u", nearest_history.sample_count);
					ImGui::Text("Total: %.3f", nearest_history.total);
					ImGui::Text("Total: %.3f", nearest_history.avg);
					ImGui::Text("Total: %.3f", nearest_history.min);
					ImGui::Text("Total: %.3f", nearest_history.max);

					ImGui::EndTooltip();

					ImDrawList* draw_list = ImPlot::GetPlotDrawList();
					ImVec2 nearest_history_marker_pos = ImPlot::PlotToPixels(nearest_history.frame_index, nearest_history.total, ImAxis_X1, ImAxis_Y1);
					draw_list->AddCircleFilled(nearest_history_marker_pos, 5.0f, IM_COL32(192, 192, 192, 192), 20);

					ImPlot::PopPlotClipRect();
				}

				ImPlot::EndPlot();
			}

			ImGui::End();
		}
	}
}

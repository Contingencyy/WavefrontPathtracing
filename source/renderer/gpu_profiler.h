#pragma once

namespace renderer
{
	struct frame_context_t;
}
struct string_t;

struct ID3D12CommandQueue;
struct ID3D12GraphicsCommandList10;

enum GPU_PROFILE_SCOPE : uint16_t
{
	GPU_PROFILE_SCOPE_TOTAL_GPU_TIME,
	GPU_PROFILE_SCOPE_TLAS_BUILD,
	GPU_PROFILE_SCOPE_PATHTRACE_MEGAKERNEL,
	GPU_PROFILE_SCOPE_PATHTRACE_WAVEFRONT_CLEAR,
	GPU_PROFILE_SCOPE_PATHTRACE_WAVEFRONT_INIT_ARGS,
	GPU_PROFILE_SCOPE_PATHTRACE_WAVEFRONT_GENERATE,
	GPU_PROFILE_SCOPE_PATHTRACE_WAVEFRONT_EXTEND,
	GPU_PROFILE_SCOPE_PATHTRACE_WAVEFRONT_SHADE,
	GPU_PROFILE_SCOPE_POST_PROCESS,
	GPU_PROFILE_SCOPE_COPY_BACKBUFFER,
	GPU_PROFILE_SCOPE_IMGUI,

	GPU_PROFILE_SCOPE_COUNT
};

enum GPU_TIMESTAMP_QUERY_TYPE : uint8_t
{
	GPU_TIMESTAMP_QUERY_TYPE_BEGIN,
	GPU_TIMESTAMP_QUERY_TYPE_END,
	
	GPU_TIMESTAMP_QUERY_TYPE_COUNT
};

struct gpu_timer_query_t
{
	uint64_t frame_index;
	GPU_PROFILE_SCOPE scope;
	GPU_TIMESTAMP_QUERY_TYPE type;
	uint32_t query_idx;
	uint64_t timestamp;

	ID3D12CommandQueue* d3d_command_queue;
};

struct gpu_profile_scope_result_t
{
	double frame_index;
	uint32_t sample_count;
	double total;
	double avg;
	double min;
	double max;
};

static const char* gpu_profile_scope_labels[GPU_PROFILE_SCOPE::GPU_PROFILE_SCOPE_COUNT] =
{
	"Total GPU Time",
	"TLAS Build",
	"Pathtrace Megakernel",
	"Wavefront Clear", "Wavefront Init Args", "Wavefront Generate", "Wavefront Extend", "Wavefront Shade",
	"Post-Process",
	"Copy Backbuffer", "ImGui"
};
static_assert(GPU_PROFILE_SCOPE::GPU_PROFILE_SCOPE_COUNT == ARRAY_SIZE(gpu_profile_scope_labels));

namespace renderer
{

	void gpu_profiler_begin_scope(frame_context_t& frame_ctx, ID3D12GraphicsCommandList10* command_list, GPU_PROFILE_SCOPE scope);
	void gpu_profiler_end_scope(frame_context_t& frame_ctx, ID3D12GraphicsCommandList10* command_list, GPU_PROFILE_SCOPE scope);
	void gpu_profiler_readback_timers();
	void gpu_profiler_parse_scope_results();

	void gpu_profiler_render_ui();

}

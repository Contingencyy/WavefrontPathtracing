#include "pch.h"
#include "d3d12_query.h"
#include "d3d12_common.h"
#include "d3d12_resource.h"
#include "d3d12_backend.h"

namespace d3d12
{
    
    void init_queries(uint32_t timestamp_query_capacity)
    {
        g_d3d->queries.timestamp_query_capacity = timestamp_query_capacity;
        g_d3d->queries.timestamp_query_at = 0;
        
        D3D12_QUERY_HEAP_DESC timestamp_query_heap_desc = {};
        timestamp_query_heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        timestamp_query_heap_desc.Count = g_d3d->queries.timestamp_query_capacity;
        timestamp_query_heap_desc.NodeMask = 0;
        DX_CHECK_HR(g_d3d->device->CreateQueryHeap(&timestamp_query_heap_desc, IID_PPV_ARGS(&g_d3d->queries.timestamp_heap)));
        g_d3d->queries.timestamp_heap->SetName(L"Timestamp Query Heap");

        ARENA_SCRATCH_SCOPE()
        {
            for (uint32_t i = 0; i < g_d3d->swapchain.back_buffer_count; ++i)
            {
                frame_context_t& frame_ctx = g_d3d->frame_ctx[i];
                
                // Create timestamp query readback buffers
                uint64_t readback_buffer_timestamps_size = g_d3d->queries.timestamp_query_capacity * sizeof(uint64_t);
                frame_ctx.readback_buffer_timestamps = create_buffer_readback(ARENA_WPRINTF(arena_scratch,
                    L"Timestamp Query Readback Buffer %u", i).buf, readback_buffer_timestamps_size);
            }
        }
    }

    void exit_queries()
    {
        g_d3d->queries.timestamp_query_at = 0;
        
        for (uint32_t i = 0; i < g_d3d->swapchain.back_buffer_count; ++i)
        {
            frame_context_t& frame_ctx = g_d3d->frame_ctx[i];
            release_object(frame_ctx.readback_buffer_timestamps);
        }
        
        release_object(g_d3d->queries.timestamp_heap);
    }

    void reset_queries()
    {
        g_d3d->queries.timestamp_query_at = 0;
    }

    uint32_t begin_query_timestamp(ID3D12GraphicsCommandList10* command_list)
    {
        ASSERT(command_list);
        
        uint32_t timestamp_index = g_d3d->queries.timestamp_query_at;
        command_list->EndQuery(g_d3d->queries.timestamp_heap, D3D12_QUERY_TYPE_TIMESTAMP, timestamp_index);
        ++g_d3d->queries.timestamp_query_at;
        
        return timestamp_index;
    }

    uint32_t end_query_timestamp(ID3D12GraphicsCommandList10* command_list)
    {
        ASSERT(command_list);
        
        uint32_t timestamp_index =g_d3d->queries.timestamp_query_at;
        command_list->EndQuery(g_d3d->queries.timestamp_heap, D3D12_QUERY_TYPE_TIMESTAMP, timestamp_index);
        ++g_d3d->queries.timestamp_query_at;
        
        return timestamp_index;
    }

    void resolve_timestamp_queries(ID3D12GraphicsCommandList10* command_list)
    {
        ASSERT(command_list);
        
        frame_context_t& frame_ctx = get_frame_context();
        command_list->ResolveQueryData(g_d3d->queries.timestamp_heap, D3D12_QUERY_TYPE_TIMESTAMP, 0,
            g_d3d->queries.timestamp_query_at, frame_ctx.readback_buffer_timestamps, 0);
    }

    uint64_t get_timestamp_frequency()
    {
        uint64_t frequency = 0;
        DX_CHECK_HR(g_d3d->command_queue_direct->GetTimestampFrequency(&frequency));

        return frequency;
    }
}

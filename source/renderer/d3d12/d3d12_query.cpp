#include "pch.h"
#include "d3d12_query.h"
#include "d3d12_common.h"
#include "d3d12_resource.h"

namespace d3d12
{
    
    void init_queries(uint32_t timestamp_query_capacity)
    {
        g_d3d->queries.timestamp_query_capacity = timestamp_query_capacity;
        g_d3d->queries.timestamp_query_at = 0;
        g_d3d->queries.copy_queue_timestamp_query_at = 0;
        
        D3D12_QUERY_HEAP_DESC timestamp_query_heap_desc = {};
        timestamp_query_heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        timestamp_query_heap_desc.Count = g_d3d->queries.timestamp_query_capacity;
        timestamp_query_heap_desc.NodeMask = 0;
        DX_CHECK_HR(g_d3d->device->CreateQueryHeap(&timestamp_query_heap_desc, IID_PPV_ARGS(&g_d3d->queries.timestamp_heap)));
        g_d3d->queries.timestamp_heap->SetName(L"Timestamp Query Heap");

        timestamp_query_heap_desc.Type = D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP;
        DX_CHECK_HR(g_d3d->device->CreateQueryHeap(&timestamp_query_heap_desc, IID_PPV_ARGS(&g_d3d->queries.copy_queue_timestamp_heap)));
        g_d3d->queries.copy_queue_timestamp_heap->SetName(L"Copy Queue Timestamp Query Heap");

        ARENA_SCRATCH_SCOPE()
        {
            for (uint32_t i = 0; i < g_d3d->swapchain.back_buffer_count; ++i)
            {
                frame_context_t& frame_ctx = g_d3d->frame_ctx[i];
                
                // Create timestamp query readback buffers
                frame_ctx.timestamp_queries_readback = create_buffer_readback(ARENA_WPRINTF(arena_scratch,
                    L"Timestamp Query Readback Buffer %u", i).buf, g_d3d->queries.timestamp_query_capacity * sizeof(uint64_t));
                frame_ctx.copy_queue_timestamp_queries_readback = create_buffer_readback(ARENA_WPRINTF(arena_scratch,
                    L"Copy Queue Timestamp Query Readback Buffer %u", i).buf, g_d3d->queries.timestamp_query_capacity * sizeof(uint64_t));
            }
        }
    }

    void exit_queries()
    {
        g_d3d->queries.timestamp_query_at = 0;
        g_d3d->queries.copy_queue_timestamp_query_at = 0;
        
        for (uint32_t i = 0; i < g_d3d->swapchain.back_buffer_count; ++i)
        {
            frame_context_t& frame_ctx = g_d3d->frame_ctx[i];
            
            DX_RELEASE_OBJECT(frame_ctx.timestamp_queries_readback);
            DX_RELEASE_OBJECT(frame_ctx.copy_queue_timestamp_queries_readback);
        }
        
        DX_RELEASE_OBJECT(g_d3d->queries.timestamp_heap);
        DX_RELEASE_OBJECT(g_d3d->queries.copy_queue_timestamp_heap);
    }

    void reset_queries()
    {
        g_d3d->queries.timestamp_query_at = 0;
        g_d3d->queries.copy_queue_timestamp_query_at = 0;
    }

    uint32_t begin_query_timestamp(ID3D12GraphicsCommandList10* command_list)
    {
        ASSERT(command_list);
        
        bool is_copy_queue = command_list->GetType() == D3D12_COMMAND_LIST_TYPE_COPY;
        ID3D12QueryHeap* query_heap = is_copy_queue ? g_d3d->queries.copy_queue_timestamp_heap : g_d3d->queries.timestamp_heap;
        uint32_t timestamp_index = is_copy_queue ? g_d3d->queries.copy_queue_timestamp_query_at : g_d3d->queries.timestamp_query_at;
        
        command_list->EndQuery(query_heap, D3D12_QUERY_TYPE_TIMESTAMP, timestamp_index);

        is_copy_queue ? g_d3d->queries.copy_queue_timestamp_query_at++ : g_d3d->queries.timestamp_query_at++;
        return timestamp_index;
    }

    uint32_t end_query_timestamp(ID3D12GraphicsCommandList10* command_list)
    {
        ASSERT(command_list);
        
        bool is_copy_queue = command_list->GetType() == D3D12_COMMAND_LIST_TYPE_COPY;
        ID3D12QueryHeap* query_heap = is_copy_queue ? g_d3d->queries.copy_queue_timestamp_heap : g_d3d->queries.timestamp_heap;
        uint32_t timestamp_index = is_copy_queue ? g_d3d->queries.copy_queue_timestamp_query_at : g_d3d->queries.timestamp_query_at;
        
        command_list->EndQuery(query_heap, D3D12_QUERY_TYPE_TIMESTAMP, timestamp_index);
        
        is_copy_queue ? g_d3d->queries.copy_queue_timestamp_query_at++ : g_d3d->queries.timestamp_query_at++;
        return timestamp_index;
    }

    void resolve_timestamp_queries(ID3D12GraphicsCommandList10* command_list)
    {
        ASSERT(command_list);
        
        bool is_copy_queue = command_list->GetType() == D3D12_COMMAND_LIST_TYPE_COPY;
        ID3D12QueryHeap* query_heap = is_copy_queue ? g_d3d->queries.copy_queue_timestamp_heap : g_d3d->queries.timestamp_heap;
        uint32_t query_count_to_resolve = is_copy_queue ? g_d3d->queries.copy_queue_timestamp_query_at : g_d3d->queries.timestamp_query_at;

        frame_context_t& frame_ctx = get_frame_context();
        ID3D12Resource* query_readback_buffer = is_copy_queue ? frame_ctx.copy_queue_timestamp_queries_readback : frame_ctx.timestamp_queries_readback;
        
        command_list->ResolveQueryData(query_heap, D3D12_QUERY_TYPE_TIMESTAMP, 0, query_count_to_resolve, query_readback_buffer, 0);
    }

    uint64_t get_timestamp_frequency()
    {
        uint64_t frequency = 0;
        DX_CHECK_HR(g_d3d->command_queue_direct->GetTimestampFrequency(&frequency));

        return frequency;
    }
}

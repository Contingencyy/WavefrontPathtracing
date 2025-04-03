#pragma once
#include "d3d12_include.h"

namespace d3d12
{

    inline constexpr uint32_t TIMESTAMP_QUERIES_DEFAULT_CAPACITY = 128;

    void init_queries(uint32_t timestamp_query_capacity);
    void exit_queries();
    
    void reset_queries();

    uint32_t begin_query_timestamp(ID3D12GraphicsCommandList10* command_list);
    uint32_t end_query_timestamp(ID3D12GraphicsCommandList10* command_list);
    void resolve_timestamp_queries(ID3D12GraphicsCommandList10* command_list);
    uint64_t get_timestamp_frequency();

}

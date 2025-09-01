#pragma once

namespace fileio
{

    struct read_file_result_t
    {
        bool loaded;
        uint64_t size;
        uint8_t* data;
    };
    
    read_file_result_t read_file(memory_arena_t& arena, const char* filepath);
    
}

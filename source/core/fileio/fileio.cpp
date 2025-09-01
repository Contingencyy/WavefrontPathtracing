#include "pch.h"
#include "fileio.h"

namespace fileio
{
    
    read_file_result_t read_file(memory_arena_t& arena, const char* filepath)
    {
        read_file_result_t ret = {};

        FILE* file = fopen(filepath, "rb");
        if (!file)
        {
            return ret;
        }

        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        ret.size = file_size;
        ret.data = (uint8_t*)ARENA_ALLOC(arena, file_size, 16);
        size_t bytes_read = fread(ret.data, 1, file_size, file);

        if (bytes_read != file_size)
        {
            fclose(file);
            return ret;
        }
        
        fclose(file);
        ret.loaded = true;

        return ret;
    }
    
}

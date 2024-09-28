#pragma once
#include "dx12_include.h"

namespace dx12_backend
{
	
	void init(memory_arena_t* arena);
	void exit();

	void begin_frame();
	void end_frame();

	void copy_to_back_buffer(ID3D12Resource* src_resource, u32 render_width, u32 render_height);
	void present();

}

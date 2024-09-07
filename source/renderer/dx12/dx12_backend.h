#pragma once

namespace dx12_backend
{

	void init(memory_arena_t* arena);
	void exit();

	void begin_frame();
	void end_frame();

	void copy_to_back_buffer(u8* ptr_pixel_data);
	void present();

}

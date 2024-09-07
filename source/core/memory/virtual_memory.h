#pragma once

// Implementation is platform-specific
namespace virtual_memory
{

	void* reserve(u64 Size);
	b8 commit(void* Address, u64 Size);
	void decommit(void* Address, u64 Size);
	void release(void* Address);

}

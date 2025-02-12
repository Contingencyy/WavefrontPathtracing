#pragma once

// Implementation is platform-specific
namespace virtual_memory
{

	void* reserve(uint64_t Size);
	bool commit(void* Address, uint64_t Size);
	void decommit(void* Address, uint64_t Size);
	void release(void* Address);

}

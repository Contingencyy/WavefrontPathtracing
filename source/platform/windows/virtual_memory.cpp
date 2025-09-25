#include "core/memory/virtual_memory.h"
#include "core/assertion.h"

#include "platform/windows/windows_common.h"

namespace virtual_memory
{

	void* reserve(uint64_t size)
	{
		void* reserved = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
		ASSERT(reserved);

		return reserved;
	}

	bool commit(void* address, uint64_t size)
	{
		void* committed = VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE);
		ASSERT(committed);

		return committed;
	}

	void decommit(void* address, uint64_t size)
	{
		int32_t status = VirtualFree(address, size, MEM_DECOMMIT);
		ASSERT(status > 0);
	}

	void release(void* address)
	{
		int32_t status = VirtualFree(address, 0, MEM_RELEASE);
		ASSERT(status > 0);
	}

}


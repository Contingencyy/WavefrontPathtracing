#include "pch.h"
#include "core/memory/virtual_memory.h"
#include "core/assertion.h"

#include "platform/windows/windows_common.h"

namespace virtual_memory
{

	void* reserve(u64 size)
	{
		void* reserved = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
		ASSERT(reserved);

		return reserved;
	}

	b8 commit(void* address, u64 size)
	{
		void* committed = VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE);
		ASSERT(committed);

		return committed;
	}

	void decommit(void* address, u64 size)
	{
		i32 status = VirtualFree(address, size, MEM_DECOMMIT);
		ASSERT(status > 0);
	}

	void release(void* address)
	{
		i32 status = VirtualFree(address, 0, MEM_RELEASE);
		ASSERT(status > 0);
	}

}


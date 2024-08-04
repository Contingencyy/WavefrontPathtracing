#include "Pch.h"
#include "Core/Memory/VirtualMemory.h"
#include "Core/Assertion.h"

#include <Windows.h>

namespace VirtualMemory
{

	void* Reserve(u64 Size)
	{
		void* reserved = VirtualAlloc(NULL, Size, MEM_RESERVE, PAGE_NOACCESS);
		ASSERT(reserved);

		return reserved;
	}

	b8 Commit(void* Address, u64 Size)
	{
		void* committed = VirtualAlloc(Address, Size, MEM_COMMIT, PAGE_READWRITE);
		ASSERT(committed);

		return committed;
	}

	void Decommit(void* Address, u64 Size)
	{
		i32 status = VirtualFree(Address, Size, MEM_DECOMMIT);
		ASSERT(status > 0);
	}

	void Release(void* Address)
	{
		i32 status = VirtualFree(Address, 0, MEM_RELEASE);
		ASSERT(status > 0);
	}

}


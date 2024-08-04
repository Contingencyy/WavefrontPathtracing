#pragma once

// Implementation is platform-specific
namespace VirtualMemory
{

	void* Reserve(u64 Size);
	b8 Commit(void* Address, u64 Size);
	void Decommit(void* Address, u64 Size);
	void Release(void* Address);

}

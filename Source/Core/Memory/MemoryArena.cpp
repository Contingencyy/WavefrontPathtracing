#include "Pch.h"
#include "MemoryArena.h"
#include "VirtualMemory.h"

static constexpr u64 ARENA_RESERVE_CHUNK_SIZE = GB(4ull);
static constexpr u64 ARENA_COMMIT_CHUNK_SIZE = KB(4ull);
static constexpr u64 ARENA_DECOMMIT_KEEP_CHUNK_SIZE = KB(4ull);

static b8 ArenaCanFitAlloc(MemoryArena* Arena, u64 Size, u64 Align)
{
	u64 AlignedBytesLeft = ALIGN_DOWN_POW2(Arena->PtrEnd - Arena->PtrAt, Align);
	return AlignedBytesLeft >= Size;
}

void* MemoryArena::Alloc(MemoryArena* Arena, u64 Size, u64 Align)
{
	// The arena holds no memory yet, so we need to reserve some
	if (!Arena->PtrBase)
	{
		Arena->PtrBase = (u8*)VirtualMemory::Reserve(ARENA_RESERVE_CHUNK_SIZE);
		Arena->PtrAt = Arena->PtrBase;
		Arena->PtrEnd = Arena->PtrBase + ARENA_RESERVE_CHUNK_SIZE;
		Arena->PtrCommitted = Arena->PtrBase;
	}

	u8* Result = nullptr;

	// Can the arena fit the allocation
	if (ArenaCanFitAlloc(Arena, Size, Align))
	{
		// Align the allocation pointer to the alignment required
		Result = (u8*)ALIGN_UP_POW2(Arena->PtrAt, Align);

		// If the arena does not have enough memory committed, make it commit memory that makes the allocation fit, aligned up to the default commit chunk size
		if (Arena->PtrAt + Size > Arena->PtrCommitted)
		{
			u64 CommitChunkSize = ALIGN_UP_POW2(Arena->PtrAt + Size - Arena->PtrCommitted, ARENA_COMMIT_CHUNK_SIZE);
			VirtualMemory::Commit(Arena->PtrCommitted, CommitChunkSize);
			Arena->PtrCommitted += CommitChunkSize;
		}

		Arena->PtrAt = Result + Size;
		ASSERT(Arena->PtrAt >= Arena->PtrBase);
		ASSERT(Arena->PtrAt < Arena->PtrEnd);
	}

	return Result;
}

void* MemoryArena::AllocZero(MemoryArena* Arena, u64 Size, u64 Align)
{
	void* Result = Alloc(Arena, Size, Align);
	memset(Result, 0, Size);
	return Result;
}

void MemoryArena::Free(MemoryArena* Arena, u8* Ptr)
{
	ASSERT(Ptr);

	// NOTE: Might leave small byte pieces if we reset the arena to a pointer
	// that was aligned in some way because the allocation before left the PtrAt not at the required alignment
	u64 BytesToFree = Arena->PtrAt - Ptr;
	u64 BytesAllocated = Arena->PtrAt - Arena->PtrBase;

	if (BytesAllocated >= BytesToFree)
	{
		// Move the current pointer back to free Size memory
		Arena->PtrAt -= BytesToFree;

		// Also decommit the freed memory, except the first ARENA_DECOMMIT_KEEP_CHUNK_SIZE bytes after the current pointer
		u8* PtrDecommit = Arena->PtrAt + ARENA_DECOMMIT_KEEP_CHUNK_SIZE;

		if (Arena->PtrCommitted > PtrDecommit)
		{
			u64 DecommitSize = Arena->PtrCommitted - PtrDecommit;
			VirtualMemory::Decommit(PtrDecommit, DecommitSize);
		}
	}
}

void MemoryArena::Clear(MemoryArena* Arena)
{
	// Reset the arena current pointer to its base
	Arena->PtrAt = Arena->PtrBase;

	// We decommit all memory except the first ARENA_DECOMMIT_KEEP_CHUNK_SIZE bytes
	u8* PtrDecommit = Arena->PtrBase + ARENA_DECOMMIT_KEEP_CHUNK_SIZE;

	if (Arena->PtrCommitted > PtrDecommit)
	{
		u64 DecommitSize = Arena->PtrCommitted - PtrDecommit;
		VirtualMemory::Decommit(PtrDecommit, DecommitSize);
	}
}

void MemoryArena::Release(MemoryArena* Arena)
{
	VirtualMemory::Release(Arena->PtrBase);
	Arena->PtrBase = Arena->PtrAt = Arena->PtrEnd = Arena->PtrCommitted = nullptr;
	Arena->TotalBytes = 0;
}

#pragma once

struct MemoryArena;

struct MemoryArenaMarker
{
	MemoryArena* Arena;
	u8* Ptr;
};

struct MemoryArena
{
	u8* PtrBase;
	u8* PtrEnd;
	u8* PtrAt;
	u8* PtrCommitted;

	static void* Alloc(MemoryArena* Arena, u64 Size, u64 Align);
	static void* AllocZero(MemoryArena* Arena, u64 Size, u64 Align);

	static void Decommit(MemoryArena* Arena, u8* Ptr);
	static void Free(MemoryArena* Arena, u8* Ptr);
	static void Clear(MemoryArena* Arena);
	static void Release(MemoryArena* Arena);

	static u64 TotalReserved(MemoryArena* Arena);
	static u64 TotalAllocated(MemoryArena* Arena);
	static u64 TotalFree(MemoryArena* Arena);
	static u64 TotalCommitted(MemoryArena* Arena);

	static void* BootstrapArena(u64 Size, u64 Align, u64 ArenaOffset);

#define ARENA_ALLOC(Arena, Size, Align) MemoryArena::Alloc(Arena, Size, Align)
#define ARENA_ALLOC_ZERO(Arena, Size, Align) MemoryArena::AllocZero(Arena, Size, Align)
#define ARENA_ALLOC_ARRAY(Arena, Type, Count) (Type *)MemoryArena::Alloc((Arena), sizeof(Type) * (Count), alignof(Type))
#define ARENA_ALLOC_ARRAY_ZERO(Arena, Type, Count) (Type *)MemoryArena::AllocZero((Arena), sizeof(Type) * (Count), alignof(Type))
#define ARENA_ALLOC_STRUCT(Arena, Type) ARENA_ALLOC_ARRAY(Arena, Type, 1)
#define ARENA_ALLOC_STRUCT_ZERO(Arena, Type) ARENA_ALLOC_ARRAY_ZERO(Arena, Type, 1)

#define ARENA_FREE(Arena, Ptr) MemoryArena::Free(Arena, Ptr)
#define ARENA_CLEAR(Arena) MemoryArena::Clear(Arena)
#define ARENA_RELEASE(Arena) MemoryArena::Release(Arena)

#define ARENA_BOOTSTRAP(Type, ArenaOffset) (Type *)MemoryArena::BootstrapArena(sizeof(Type), alignof(Type), ArenaOffset)

#define ARENA_MEMORY_SCOPE(Arena) \
	for (u8* ArenaMarker = (Arena)->PtrAt; (Arena) && ArenaMarker; ARENA_FREE((Arena), ArenaMarker), ArenaMarker = nullptr)

};

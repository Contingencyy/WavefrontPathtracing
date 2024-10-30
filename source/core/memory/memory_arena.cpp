#include "pch.h"
#include "memory_arena.h"
#include "virtual_memory.h"

static constexpr u64 ARENA_RESERVE_CHUNK_SIZE = GB(4ull);
static constexpr u64 ARENA_COMMIT_CHUNK_SIZE = KB(4ull);
static constexpr u64 ARENA_DECOMMIT_KEEP_CHUNK_SIZE = KB(4ull);

static b8 arena_can_fit(memory_arena_t* arena, u64 size, u64 align)
{
	u64 AlignedBytesLeft = ALIGN_DOWN_POW2(arena->ptr_end - arena->ptr_at, align);
	return AlignedBytesLeft >= size;
}

void memory_arena_t::init(memory_arena_t* arena)
{
	if (!arena->ptr_base)
	{
		arena->ptr_base = (u8*)virtual_memory::reserve(ARENA_RESERVE_CHUNK_SIZE);
		arena->ptr_at = arena->ptr_base;
		arena->ptr_end = arena->ptr_base + ARENA_RESERVE_CHUNK_SIZE;
		arena->ptr_committed = arena->ptr_base;
	}
}

void* memory_arena_t::alloc(memory_arena_t* arena, u64 size, u64 align)
{
	// The arena holds no memory yet, so we need to reserve some
	init(arena);

	u8* result = nullptr;

	// Can the arena fit the allocation
	if (arena_can_fit(arena, size, align))
	{
		// align the allocation pointer to the alignment required
		result = (u8*)ALIGN_UP_POW2(arena->ptr_at, align);

		// If the arena does not have enough memory committed, make it commit memory that makes the allocation fit, aligned up to the default commit chunk size
		if (arena->ptr_at + size > arena->ptr_committed)
		{
			u64 commit_chunk_size = ALIGN_UP_POW2(arena->ptr_at + size - arena->ptr_committed, ARENA_COMMIT_CHUNK_SIZE);
			virtual_memory::commit(arena->ptr_committed, commit_chunk_size);
			arena->ptr_committed += commit_chunk_size;
		}

		arena->ptr_at = result + size;
		ASSERT(arena->ptr_at >= arena->ptr_base);
		ASSERT(arena->ptr_at < arena->ptr_end);
	}

	return result;
}

void* memory_arena_t::alloc_zero(memory_arena_t* arena, u64 size, u64 align)
{
	void* result = alloc(arena, size, align);
	memset(result, 0, size);
	return result;
}

void memory_arena_t::decommit(memory_arena_t* arena, u8* ptr)
{
	ASSERT(ptr);

	// Decommit memory until ptr, aligned to ARENA_COMMIT_CHUNK_SIZE
	u8* ptr_decommit = (u8*)ALIGN_UP_POW2(ptr, ARENA_COMMIT_CHUNK_SIZE);
	u64 decommit_bytes = MAX(0, arena->ptr_committed - ptr_decommit);

	if (decommit_bytes > 0)
	{
		virtual_memory::decommit(ptr_decommit, decommit_bytes);
		arena->ptr_committed = ptr_decommit;
	}
}

void memory_arena_t::free(memory_arena_t* arena, u8* ptr)
{
	ASSERT(ptr);

	// NOTE: Might leave small byte pieces if we reset the arena to a pointer
	// that was aligned in some way because the allocation before left the ptr_at not at the required alignment
	u64 bytes_to_free = arena->ptr_at - ptr;
	u64 bytes_allocated = arena->ptr_at - arena->ptr_base;

	if (bytes_allocated >= bytes_to_free)
	{
		// Move the current pointer back to free size memory
		arena->ptr_at -= bytes_to_free;
		memory_arena_t::decommit(arena, arena->ptr_at);
	}
}

void memory_arena_t::clear(memory_arena_t* arena)
{
	// reset the arena current pointer to its base
	arena->ptr_at = arena->ptr_base;
	memory_arena_t::decommit(arena, arena->ptr_base);
}

void memory_arena_t::release(memory_arena_t* arena)
{
	void* ptr_arena_base = arena->ptr_base;
	arena->ptr_base = arena->ptr_at = arena->ptr_end = arena->ptr_committed = nullptr;
	virtual_memory::release(ptr_arena_base);
}

u64 memory_arena_t::total_reserved(memory_arena_t* arena)
{
	return arena->ptr_end - arena->ptr_base;
}

u64 memory_arena_t::total_allocated(memory_arena_t* arena)
{
	return arena->ptr_at - arena->ptr_base;
}

u64 memory_arena_t::total_free(memory_arena_t* arena)
{
	return arena->ptr_committed - arena->ptr_at;
}

u64 memory_arena_t::total_committed(memory_arena_t* arena)
{
	return arena->ptr_committed - arena->ptr_base;
}

void* memory_arena_t::bootstrap_arena(u64 size, u64 align, u64 arena_offset)
{
	memory_arena_t arena = {};
	void* result = ARENA_ALLOC_ZERO(&arena, size, align);
	memcpy((u8*)result + arena_offset, &arena, sizeof(memory_arena_t));

	return result;
}

memory_arena_t* memory_arena_t::get_scratch()
{
	thread_local memory_arena_t g_arena_thread;

	// Per thread scratch arena, only used with memory scopes to always automatically reset them
	init(&g_arena_thread);
	return &g_arena_thread;
}

string_t memory_arena_t::printf(memory_arena_t* arena, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	string_t result = printf_args(arena, fmt, args);

	va_end(args);
	return result;
}

string_t memory_arena_t::printf_args(memory_arena_t* arena, const char* fmt, va_list args)
{
	va_list args2;
	va_copy(args2, args);

	u32 count = vsnprintf(NULL, 0, fmt, args2);

	va_end(args2);

	string_t result = string::make(arena, count + 1);
	vsnprintf(result.buf, count + 1, fmt, args);

	return result;
}

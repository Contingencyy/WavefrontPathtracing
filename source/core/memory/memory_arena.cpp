#include "pch.h"
#include "memory_arena.h"
#include "virtual_memory.h"

static constexpr uint64_t ARENA_RESERVE_CHUNK_SIZE = GB(4ull);
static constexpr uint64_t ARENA_COMMIT_CHUNK_SIZE = KB(4ull);
static constexpr uint64_t ARENA_DECOMMIT_KEEP_CHUNK_SIZE = KB(4ull);

static bool arena_can_fit(memory_arena_t& arena, uint64_t size, uint64_t align)
{
	uint64_t AlignedBytesLeft = ALIGN_DOWN_POW2(arena.ptr_end - arena.ptr_at, align);
	return AlignedBytesLeft >= size;
}

void memory_arena::init(memory_arena_t& arena)
{
	if (!arena.ptr_base)
	{
		arena.ptr_base = (uint8_t*)virtual_memory::reserve(ARENA_RESERVE_CHUNK_SIZE);
		arena.ptr_at = arena.ptr_base;
		arena.ptr_end = arena.ptr_base + ARENA_RESERVE_CHUNK_SIZE;
		arena.ptr_committed = arena.ptr_base;
	}
}

void* memory_arena::alloc(memory_arena_t& arena, uint64_t size, uint64_t align)
{
	// The arena holds no memory yet, so we need to reserve some
	init(arena);

	uint8_t* result = nullptr;

	// Can the arena fit the allocation
	if (arena_can_fit(arena, size, align))
	{
		// align the allocation pointer to the alignment required
		result = (uint8_t*)ALIGN_UP_POW2(arena.ptr_at, align);

		// If the arena does not have enough memory committed, make it commit memory that makes the allocation fit, aligned up to the default commit chunk size
		if (arena.ptr_at + size > arena.ptr_committed)
		{
			uint64_t commit_chunk_size = ALIGN_UP_POW2(arena.ptr_at + size - arena.ptr_committed, ARENA_COMMIT_CHUNK_SIZE);
			virtual_memory::commit(arena.ptr_committed, commit_chunk_size);
			arena.ptr_committed += commit_chunk_size;
		}

		arena.ptr_at = result + size;
		ASSERT(arena.ptr_at >= arena.ptr_base);
		ASSERT(arena.ptr_at < arena.ptr_end);
	}

	return result;
}

void* memory_arena::alloc_zero(memory_arena_t& arena, uint64_t size, uint64_t align)
{
	void* result = alloc(arena, size, align);
	memset(result, 0, size);
	return result;
}

void memory_arena::decommit(memory_arena_t& arena, uint8_t* ptr)
{
	ASSERT(ptr);

	// Decommit memory until ptr, aligned to ARENA_DECOMMIT_KEEP_CHUNK_SIZE
	uint8_t* ptr_decommit = (uint8_t*)ALIGN_UP_POW2(ptr, ARENA_DECOMMIT_KEEP_CHUNK_SIZE);
	uint64_t decommit_bytes = MAX(0, arena.ptr_committed - ptr_decommit);

	if (decommit_bytes > 0)
	{
		virtual_memory::decommit(ptr_decommit, decommit_bytes);
		arena.ptr_committed = ptr_decommit;
	}
}

void memory_arena::free(memory_arena_t& arena, uint8_t* ptr)
{
	ASSERT(ptr);

	// NOTE: Might leave small byte pieces if we reset the arena to a pointer
	// that was aligned in some way because the allocation before left the ptr_at not at the required alignment
	uint64_t bytes_to_free = arena.ptr_at - ptr;
	uint64_t bytes_allocated = arena.ptr_at - arena.ptr_base;

	if (bytes_allocated >= bytes_to_free)
	{
		// Move the current pointer back to free size memory
		arena.ptr_at -= bytes_to_free;
		memory_arena::decommit(arena, arena.ptr_at);
	}
}

void memory_arena::clear(memory_arena_t& arena)
{
	// reset the arena current pointer to its base
	arena.ptr_at = arena.ptr_base;
	
	if (arena.ptr_base)
	{
		memory_arena::decommit(arena, arena.ptr_base);
	}
}

void memory_arena::release(memory_arena_t& arena)
{
	void* ptr_arena_base = arena.ptr_base;
	arena.ptr_base = arena.ptr_at = arena.ptr_end = arena.ptr_committed = nullptr;
	
	if (ptr_arena_base)
	{
		virtual_memory::release(ptr_arena_base);
	}
}

uint64_t memory_arena::total_reserved(memory_arena_t& arena)
{
	return arena.ptr_end - arena.ptr_base;
}

uint64_t memory_arena::total_allocated(memory_arena_t& arena)
{
	return arena.ptr_at - arena.ptr_base;
}

uint64_t memory_arena::total_free(memory_arena_t& arena)
{
	return arena.ptr_committed - arena.ptr_at;
}

uint64_t memory_arena::total_committed(memory_arena_t& arena)
{
	return arena.ptr_committed - arena.ptr_base;
}

void* memory_arena::bootstrap_arena(uint64_t size, uint64_t align, uint64_t arena_offset)
{
	memory_arena_t arena = {};
	void* result = ARENA_ALLOC_ZERO(arena, size, align);
	memcpy((uint8_t*)result + arena_offset, &arena, sizeof(memory_arena_t));

	return result;
}

memory_arena_t& memory_arena::get_scratch()
{
	thread_local memory_arena_t g_arena_thread;

	// Per thread scratch arena, only used with memory scopes to always automatically reset them
	init(g_arena_thread);
	return g_arena_thread;
}

string_t memory_arena::printf(memory_arena_t& arena, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	string_t result = printf_args(arena, fmt, args);

	va_end(args);
	return result;
}

string_t memory_arena::printf_args(memory_arena_t& arena, const char* fmt, va_list args)
{
	va_list args2;
	va_copy(args2, args);

	uint32_t count = vsnprintf(NULL, 0, fmt, args2);
	
	va_end(args2);

	string_t result = string::make(arena, count + 1);
	vsnprintf(result.buf, count + 1, fmt, args);

	return result;
}

wstring_t memory_arena::wprintf(memory_arena_t& arena, const wchar_t* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	wstring_t result = wprintf_args(arena, fmt, args);

	va_end(args);
	return result;
}

wstring_t memory_arena::wprintf_args(memory_arena_t& arena, const wchar_t* fmt, va_list args)
{
	va_list args2;
	va_copy(args2, args);

	uint32_t count = _vscwprintf(fmt, args2);
	
	va_end(args2);

	wstring_t result = wstring::make(arena, count + 1);
	_vsnwprintf(result.buf, count + 1, fmt, args);

	return result;
}

string_t memory_arena::wide_to_char(memory_arena_t& arena, const wchar_t* wide)
{
	size_t len = wcslen(wide);
	string_t str = string::make(arena, len);
	wcstombs(str.buf, wide, str.count);

	return str;
}

wstring_t memory_arena::char_to_wide(memory_arena_t& arena, const char* str)
{
	size_t len = strlen(str);
	wstring_t wstr = wstring::make(arena, len);
	mbstowcs(wstr.buf, str, wstr.count);

	return wstr;
}

void memory_arena::string_copy(memory_arena_t& arena, const string_t& src, string_t& dst)
{
	string_copy_sub(arena, src, 0, src.count, dst);
}

void memory_arena::string_copy_sub(memory_arena_t& arena, const string_t& src, uint32_t offset, uint32_t count, string_t& dst)
{
	ASSERT_MSG(src.buf, "Tried to copy string but source string is nullptr");
	ASSERT_MSG(src.count <= offset + count, "Tried to copy a substring but range exceeded source string");

	if (!dst.buf)
	{
		dst.buf = ARENA_ALLOC_ARRAY(arena, char, offset + count);
	}

	memcpy(dst.buf, src.buf + offset + count, src.count * sizeof(char));
}

void memory_arena::wstring_copy(memory_arena_t& arena, const wstring_t& src, wstring_t& dst)
{
	wstring_copy_sub(arena, src, 0, src.count, dst);
}

void memory_arena::wstring_copy_sub(memory_arena_t& arena, const wstring_t& src, uint32_t offset, uint32_t count, wstring_t& dst)
{
	ASSERT_MSG(src.buf, "Tried to copy string but source string is nullptr");
	ASSERT_MSG(src.count <= offset + count, "Tried to copy a substring but range exceeded source string");

	if (!dst.buf)
	{
		dst.buf = ARENA_ALLOC_ARRAY(arena, wchar_t, count);
	}

	dst.count = count;
	memcpy(dst.buf, src.buf + offset, src.count * sizeof(wchar_t));
}

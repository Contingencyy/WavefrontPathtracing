#pragma once
#include "core/common.h"
#include "core/string/string.h"

struct memory_arena_t;

struct memory_arena_marker_t
{
	memory_arena_t* arena;
	u8* ptr;
};

struct memory_arena_t
{
	u8* ptr_base;
	u8* ptr_end;
	u8* ptr_at;
	u8* ptr_committed;
};

namespace memory_arena
{

	void init(memory_arena_t* arena);

	void* alloc(memory_arena_t* arena, u64 size, u64 align);
	void* alloc_zero(memory_arena_t* arena, u64 size, u64 align);

	void decommit(memory_arena_t* arena, u8* ptr);
	void free(memory_arena_t* arena, u8* ptr);
	void clear(memory_arena_t* arena);
	void release(memory_arena_t* arena);

	u64 total_reserved(memory_arena_t* arena);
	u64 total_allocated(memory_arena_t* arena);
	u64 total_free(memory_arena_t* arena);
	u64 total_committed(memory_arena_t* arena);

	void* bootstrap_arena(u64 size, u64 align, u64 arena_offset);
	memory_arena_t* get_scratch();

	string_t printf(memory_arena_t* arena, const char* fmt, ...);
	string_t printf_args(memory_arena_t* arena, const char* fmt, va_list args);
	wstring_t wprintf(memory_arena_t* arena, const wchar_t* fmt, ...);
	wstring_t wprintf_args(memory_arena_t* arena, const wchar_t* fmt, va_list args);
	string_t wide_to_char(memory_arena_t* arena, const wchar_t* wide);
	wstring_t char_to_wide(memory_arena_t* arena, const char* str);

#define ARENA_ALLOC(arena, size, align) memory_arena::alloc(arena, size, align)
#define ARENA_ALLOC_ZERO(arena, size, align) memory_arena::alloc_zero(arena, size, align)
#define ARENA_ALLOC_ARRAY(arena, type, count) (type *)memory_arena::alloc((arena), sizeof(type) * (count), alignof(type))
#define ARENA_ALLOC_ARRAY_ZERO(arena, type, count) (type *)memory_arena::alloc_zero((arena), sizeof(type) * (count), alignof(type))
#define ARENA_ALLOC_STRUCT(arena, type) ARENA_ALLOC_ARRAY(arena, type, 1)
#define ARENA_ALLOC_STRUCT_ZERO(arena, type) ARENA_ALLOC_ARRAY_ZERO(arena, type, 1)

	//	template<typename T>
	//	static T* alloc_default(memory_arena_t* arena)
	//	{
	//		T* alloc = ARENA_ALLOC_STRUCT(arena, T);
	//		*alloc = T();
	//		return alloc;
	//	}
	//#define ARENA_ALLOC_STRUCT_DEFAULT(arena, type) memory_arena_t::alloc_default<type>(arena)

#define ARENA_FREE(arena, ptr) memory_arena::free(arena, ptr)
#define ARENA_CLEAR(arena) memory_arena::clear(arena)
#define ARENA_RELEASE(arena) memory_arena::release(arena)

#define ARENA_BOOTSTRAP(type, arena_offset) (type *)memory_arena::bootstrap_arena(sizeof(type), alignof(type), arena_offset)

#define ARENA_MEMORY_SCOPE(arena) \
	for (u8* arena_marker = (arena)->ptr_at; (arena) && arena_marker; ARENA_FREE((arena), arena_marker), arena_marker = nullptr)
#define ARENA_SCRATCH_SCOPE() memory_arena_t* arena_scratch = memory_arena::get_scratch(); ARENA_MEMORY_SCOPE(arena_scratch)

#define ARENA_PRINTF(arena, fmt, ...) memory_arena::printf(arena, fmt, ##__VA_ARGS__)
#define ARENA_PRINTF_ARGS(arena, fmt, args) memory_arena::printf_args(arena, fmt, args)
#define ARENA_WPRINTF(arena, fmt, ...) memory_arena::wprintf(arena, fmt, ##__VA_ARGS__)
#define ARENA_WPRINTF_ARGS(arena, fmt, args) memory_arena::wprintf_args(arena, fmt, args)
#define ARENA_WIDE_TO_CHAR(arena, wide) memory_arena::wide_to_char(arena, wide)
#define ARENA_CHAR_TO_WIDE(arena, str) memory_arena::char_to_wide(arena, str)

}

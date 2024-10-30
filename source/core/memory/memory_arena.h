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

	static void init(memory_arena_t* arena);

	static void* alloc(memory_arena_t* arena, u64 size, u64 align);
	static void* alloc_zero(memory_arena_t* arena, u64 size, u64 align);

	static void decommit(memory_arena_t* arena, u8* ptr);
	static void free(memory_arena_t* arena, u8* ptr);
	static void clear(memory_arena_t* arena);
	static void release(memory_arena_t* arena);

	static u64 total_reserved(memory_arena_t* arena);
	static u64 total_allocated(memory_arena_t* arena);
	static u64 total_free(memory_arena_t* arena);
	static u64 total_committed(memory_arena_t* arena);

	static void* bootstrap_arena(u64 size, u64 align, u64 arena_offset);
	static memory_arena_t* get_scratch();

	static string_t printf(memory_arena_t* arena, const char* fmt, ...);
	static string_t printf_args(memory_arena_t* arena, const char* fmt, va_list args);

#define ARENA_ALLOC(arena, size, align) memory_arena_t::alloc(arena, size, align)
#define ARENA_ALLOC_ZERO(arena, size, align) memory_arena_t::alloc_zero(arena, size, align)
#define ARENA_ALLOC_ARRAY(arena, type, count) (type *)memory_arena_t::alloc((arena), sizeof(type) * (count), alignof(type))
#define ARENA_ALLOC_ARRAY_ZERO(arena, type, count) (type *)memory_arena_t::alloc_zero((arena), sizeof(type) * (count), alignof(type))
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

#define ARENA_FREE(arena, ptr) memory_arena_t::free(arena, ptr)
#define ARENA_CLEAR(arena) memory_arena_t::clear(arena)
#define ARENA_RELEASE(arena) memory_arena_t::release(arena)

#define ARENA_BOOTSTRAP(type, arena_offset) (type *)memory_arena_t::bootstrap_arena(sizeof(type), alignof(type), arena_offset)

#define ARENA_MEMORY_SCOPE(arena) \
	for (u8* arena_marker = (arena)->ptr_at; (arena) && arena_marker; ARENA_FREE((arena), arena_marker), arena_marker = nullptr)
#define ARENA_SCRATCH_SCOPE() memory_arena_t* arena_scratch = memory_arena_t::get_scratch(); ARENA_MEMORY_SCOPE(arena_scratch)

#define ARENA_PRINTF(arena, fmt, ...) memory_arena_t::printf(arena, fmt, ##__VA_ARGS__)
#define ARENA_PRINTF_ARGS(arena, fmt, args) memory_arena_t::printf_args(arena, fmt, args)
	// TODO: wide string support for printf
};

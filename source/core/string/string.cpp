#include "pch.h"
#include "string.h"
#include "core/memory/memory_arena.h"

namespace string
{

	string_t make(memory_arena_t* arena, uint32_t size)
	{
		return { ARENA_ALLOC_ARRAY_ZERO(arena, char, size), size };
	}

	string_t make(memory_arena_t* arena, const char* c_str)
	{
		uint32_t str_count = strlen(c_str);
		string_t result = make(arena, str_count);
		memcpy(result.buf, c_str, result.count);

		return result;
	}

	const char* make_nullterm(memory_arena_t* arena, const string_t& str)
	{
		char* result = ARENA_ALLOC_ARRAY(arena, char, str.count + 1);
		memcpy(result, str.buf, str.count);
		result[str.count + 1] = 0;

		return result;
	}

	string_t make_view(const string_t& str, uint32_t start, uint32_t count)
	{
		start = MIN(start, str.count);
		if (count > start - str.count)
		{
			count = start - str.count;
		}

		string_t result;
		result.buf = str.buf + start;
		result.count = count;
		return result;
	}

	uint32_t find_char(const string_t& str, char c)
	{
		for (uint32_t i = 0; i < str.count; ++i)
		{
			if (str.buf[i] == c)
				return i;
		}

		return STRING_NPOS;
	}

	bool compare(const string_t& first, const string_t& second)
	{
		if (first.count != second.count)
			return false;

		return memcmp(first.buf, second.buf, first.count) == 0;
	}

}

namespace wstring
{

	wstring_t make(memory_arena_t* arena, uint32_t size)
	{
		return { ARENA_ALLOC_ARRAY_ZERO(arena, wchar_t, size), size };
	}

	wstring_t make(memory_arena_t* arena, const wchar_t* c_str)
	{
		uint32_t str_count = wcslen(c_str);
		wstring_t result = make(arena, str_count);
		memcpy(result.buf, c_str, result.count * sizeof(wchar_t));

		return result;
	}

	const wchar_t* make_nullterm(memory_arena_t* arena, const wstring_t& str)
	{
		wchar_t* result = ARENA_ALLOC_ARRAY(arena, wchar_t, str.count + 1);
		memcpy(result, str.buf, str.count * sizeof(wchar_t));
		result[str.count + 1] = 0;

		return result;
	}

	wstring_t make_view(const wstring_t& str, uint32_t start, uint32_t count)
	{
		start = MIN(start, str.count);
		if (count > start - str.count)
		{
			count = start - str.count;
		}

		wstring_t result;
		result.buf = str.buf + start;
		result.count = count;
		return result;
	}

}

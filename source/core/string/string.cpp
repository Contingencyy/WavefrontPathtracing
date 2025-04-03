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

	void copy(memory_arena_t* arena, string_t& dst, const string_t& src)
	{
		if (!dst.buf)
		{
			dst.buf = ARENA_ALLOC_ARRAY(arena, char, src.count);
			dst.count = src.count;
		}

		ASSERT_MSG(dst.count >= src.count, "Destination buffer is too small for source string");
		memcpy(dst.buf, src.buf, src.count);
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

	void copy(memory_arena_t* arena, wstring_t& dst, const wstring_t& src)
	{
		if (!dst.buf)
		{
			dst.buf = ARENA_ALLOC_ARRAY(arena, wchar_t, src.count);
			dst.count = src.count;
		}
		
		ASSERT_MSG(dst.count >= src.count, "Destination buffer is too small for source string");
		memcpy(dst.buf, src.buf, src.count * sizeof(wchar_t));
	}

}

//namespace wstring_builder
//{
//
//	static void add_node(wstring_builder_t& builder)
//	{
//		if (!builder.list_begin)
//		{
//			builder.list_begin = ARENA_ALLOC_STRUCT(builder.arena, wstring_builder_t::node_t);
//			builder.list = builder.list_begin;
//		}
//		else
//		{
//			builder.list->next = ARENA_ALLOC_STRUCT(builder.arena, wstring_builder_t::node_t);
//			builder.list = builder.list->next;
//		}
//	}
//
//	wstring_builder_t make(memory_arena_t* arena)
//	{
//		wstring_builder_t builder = {};
//		builder.arena = arena;
//
//		return builder;
//	}
//
//	void append(wstring_builder_t& builder, const wchar_t* c_str)
//	{
//		add_node(builder);
//		builder.list->str = wstring::make(builder.arena, c_str);
//	}
//
//	void append(wstring_builder_t& builder, const wstring_t& str)
//	{
//		add_node(builder);
//		wstring::copy(builder.arena, builder.list->str, str);
//	}
//
//	wstring_t extract(const wstring_builder_t& builder, bool add_separator, wchar_t separator)
//	{
//		return extract(builder, builder.arena, add_separator, separator);
//	}
//
//	wstring_t extract(const wstring_builder_t& builder, memory_arena_t* arena, bool add_separator, wchar_t separator)
//	{
//		uint32_t str_size = 0;
//
//		const wstring_builder_t::node_t* current = builder.list_begin;
//		while (current != nullptr)
//		{
//			str_size += current->str.count;
//			if (add_separator)
//			{
//				str_size += 1;
//			}
//			current = current->next;
//		}
//
//		static wstring_t str_result = {};
//		str_result.buf = ARENA_ALLOC_ARRAY(arena, wchar_t, str_size);
//		str_result.count = str_size;
//		uint32_t dst_offset = 0;
//
//		current = builder.list_begin;
//		while (current != nullptr)
//		{
//			memcpy(PTR_OFFSET(str_result.buf, dst_offset * sizeof(wchar_t)), current->str.buf, current->str.count * sizeof(wchar_t));
//			dst_offset += current->str.count;
//
//			str_result.buf[dst_offset] = separator;
//			dst_offset += 1;
//
//			current = current->next;
//		}
//
//		return str_result;
//	}
//
//}

bool string_t::operator==(const string_t& other) const
{
	return (this->count == other.count &&
		memcmp(this->buf, other.buf, this->count * sizeof(char)) == 0);
}

bool wstring_t::operator==(const wstring_t& other) const
{
	return (this->count == other.count &&
		memcmp(this->buf, other.buf, this->count * sizeof(wchar_t)) == 0);
}

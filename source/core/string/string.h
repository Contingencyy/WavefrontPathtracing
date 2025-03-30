#pragma once

#define STRING_NPOS 0xFFFFFFFF
// Used to printf counted strings
#define STRING_EXPAND(str) (int32_t)(str).count, (str).buf

#define STRING_LITERAL(str) string_t{ .buf = (char*)str, .count = sizeof(str) - 1 }
#define WSTRING_LITERAL(wstr) wstring_t{ .buf = (wchar_t*)wstr, .count = (sizeof(wstr) / 2) - 1 }

struct memory_arena_t;

struct string_t
{
	char* buf = nullptr;
	uint32_t count = 0;
};

struct wstring_t
{
	wchar_t* buf = nullptr;
	uint32_t count = 0;
};

namespace string
{

	string_t make(memory_arena_t* arena, uint32_t size);
	string_t make(memory_arena_t* arena, const char* c_str);
	const char* make_nullterm(memory_arena_t* arena, const string_t& str);
	string_t make_view(const string_t& str, uint32_t start, uint32_t count);

	void copy(memory_arena_t* arena, string_t& dst, const string_t& src);

	uint32_t find_char(const string_t& str, char c);
	bool compare(const string_t& first, const string_t& second);

}

namespace wstring
{

	wstring_t make(memory_arena_t* arena, uint32_t size);
	wstring_t make(memory_arena_t* arena, const wchar_t* c_str);
	const wchar_t* make_nullterm(memory_arena_t* arena, const wstring_t& str);
	wstring_t make_view(const wstring_t& str, uint32_t start, uint32_t count);

	void copy(memory_arena_t* arena, wstring_t& dst, const wstring_t& src);

}

//struct wstring_builder_t
//{
//
//	struct node_t
//	{
//		wstring_t str;
//		node_t* next = nullptr;
//	};
//
//	memory_arena_t* arena = nullptr;
//	node_t* list;
//	node_t* list_begin;
//
//};
//
//namespace wstring_builder
//{
//
//	wstring_builder_t make(memory_arena_t* arena);
//
//	void append(wstring_builder_t& builder, const wchar_t* c_str);
//	void append(wstring_builder_t& builder, const wstring_t& str);
//
//	wstring_t extract(const wstring_builder_t& builder, bool add_separator = false, wchar_t separator = L' ');
//	wstring_t extract(const wstring_builder_t& builder, memory_arena_t* arena, bool add_separator = false, wchar_t separator = L' ');
//
//}

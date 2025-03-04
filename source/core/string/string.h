#pragma once

#define STRING_NPOS 0xFFFFFFFF
#define STRING_LITERAL(str) string_t{ .buf = (char*)str, .count = sizeof(str) - 1 }
#define WSTRING_LITERAL(wstr) wstring_t{ .buf = (wchar_t*)wstr, .count = sizeof(wstr) - 1 }

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

	uint32_t find_char(const string_t& str, char c);
	bool compare(const string_t& first, const string_t& second);

}

namespace wstring
{

	wstring_t make(memory_arena_t* arena, uint32_t size);
	wstring_t make(memory_arena_t* arena, const wchar_t* c_str);
	const wchar_t* make_nullterm(memory_arena_t* arena, const wstring_t& str);
	wstring_t make_view(const wstring_t& str, uint32_t start, uint32_t count);

}

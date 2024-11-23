#pragma once

#define STRING_NPOS 0xFFFFFFFF
#define STRING_LITERAL(str) string_t{ .buf = (char*)str, .count = sizeof(str) - 1 }
#define WSTRING_LITERAL(wstr) wstring_t{ .buf = (wchar_t*)wstr, .count = sizeof(wstr) - 1 }

struct memory_arena_t;

struct string_t
{
	char* buf = nullptr;
	u32 count = 0;
};

struct wstring_t
{
	wchar_t* buf = nullptr;
	u32 count = 0;
};

namespace string
{

	string_t make(memory_arena_t* arena, u32 size);
	string_t make(memory_arena_t* arena, const char* c_str);
	const char* make_nullterm(memory_arena_t* arena, const string_t& str);
	string_t make_view(const string_t& str, u32 start, u32 count);

	u32 find_char(const string_t& str, char c);
	b8 compare(const string_t& first, const string_t& second);

}

namespace wstring
{

	wstring_t make(memory_arena_t* arena, u32 size);
	wstring_t make(memory_arena_t* arena, const wchar_t* c_str);
	const wchar_t* make_nullterm(memory_arena_t* arena, const wstring_t& str);
	wstring_t make_view(const wstring_t& str, u32 start, u32 count);

}

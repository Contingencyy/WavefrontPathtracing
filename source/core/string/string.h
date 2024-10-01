#pragma once

struct memory_arena_t;

struct string_t
{
	char* buf = nullptr;
	u32 count = 0;

	static string_t make(memory_arena_t* arena, u32 size);
	static string_t make(memory_arena_t* arena, const char* c_str);
	static const char* make_nullterm(memory_arena_t* arena, const string_t& str);
	static string_t make_view(const string_t& str, u32 start, u32 count);

	static u32 find_char(const string_t& str, char c);
	static b8 compare(const string_t& first, const string_t& second);
};

struct wstring_t
{
	wchar_t* buf = nullptr;
	u32 count = 0;

	static wstring_t make(memory_arena_t* arena, u32 size);
	static wstring_t make(memory_arena_t* arena, const wchar_t* c_str);
	static const wchar_t* make_nullterm(memory_arena_t* arena, const wstring_t& str);
	static wstring_t make_view(const wstring_t& str, u32 start, u32 count);
};

#define STRING_NPOS 0xFFFFFFF
#define STRING_LITERAL(str) string_t{ .buf = (char*)str, .count = sizeof(str) - 1 }
#define WSTRING_LITERAL(wstr) wstring_t{ .buf = (wchar_t*)wstr, .count = sizeof(wstr) - 1 }

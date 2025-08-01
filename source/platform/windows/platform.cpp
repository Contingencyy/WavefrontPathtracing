﻿#include "pch.h"
#include "platform/windows/windows_common.h"

#include "core/application.h"
#include "core/logger.h"
#include "core/input.h"
#include "core/string/string.h"

#include "imgui/imgui_impl_win32.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT Message, WPARAM WParam, LPARAM LParam);

#include <io.h>
#include <fcntl.h>

namespace platform
{

	struct internal_t
	{
		HWND hwnd;
		LONGLONG perf_freq;

		HANDLE conin;
		HANDLE conout;

		FILE* filein;
		FILE* fileout;
		FILE* fileerr;
	} static internal;
	
	static void console_create()
	{
		if (!AllocConsole())
			FATAL_ERROR("Console", "Failed to allocate console");

		SetConsoleTitle("Wavefront Pathtracer Console");
		freopen_s(&internal.filein, "CONIN$", "r", stdin);
		freopen_s(&internal.fileout, "CONOUT$", "w", stdout);
		freopen_s(&internal.fileerr, "CONOUT$", "w", stderr);

		/*HANDLE handle_con_in = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		HANDLE handle_con_out = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);*/

		SetStdHandle(STD_INPUT_HANDLE, internal.filein);
		SetStdHandle(STD_OUTPUT_HANDLE, internal.fileout);
		SetStdHandle(STD_ERROR_HANDLE, internal.fileerr);

		internal.conin = GetStdHandle(STD_INPUT_HANDLE);
		internal.conout = GetStdHandle(STD_OUTPUT_HANDLE);
	}

	static void console_destroy()
	{
		/*DeleteFile("CONIN$");
		DeleteFile("CONOUT$");*/

		fclose(internal.filein);
		fclose(internal.fileout);
		fclose(internal.fileerr);

		if (!FreeConsole())
			FATAL_ERROR("Console", "Failed to free console");
	}
	
	LRESULT WINAPI window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
			return true;

		switch (msg)
		{
		case WM_SIZE:
		case WM_SIZING:
		{
			// TODO: handle resizing swap chain and everything else that needs to happen when the window is resized
		} break;
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		{
			input::on_platform_key_button_state_changed(wparam, true);
		} break;
		case WM_SYSKEYUP:
		case WM_KEYUP:
		{
			input::on_platform_key_button_state_changed(wparam, false);
		} break;
		case WM_LBUTTONUP:
		{
			input::on_platform_key_button_state_changed(VK_LBUTTON, false);
		} break;
		case WM_RBUTTONUP:
		{
			input::on_platform_key_button_state_changed(VK_RBUTTON, false);
		} break;
		case WM_MOUSEWHEEL:
		{
			float wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
			input::on_mousewheel_scrolled(wheel_delta);
		} break;
		case WM_SETFOCUS:
		{
			input::set_window_focus(true);
		} break;
		case WM_KILLFOCUS:
		{
			platform::window_set_capture_mouse(false);
			input::set_window_focus(false);
		} break;
		case WM_DESTROY:
		{
			PostQuitMessage(0);
		} break;
		default:
		{
			return DefWindowProcW(hwnd, msg, wparam, lparam);
		} break;
		}

		return 0;
	}

	void window_get_client_area(int32_t& out_window_width, int32_t& out_window_height)
	{
		RECT client_rect = {};
		GetClientRect(internal.hwnd, &client_rect);

		out_window_width = client_rect.right - client_rect.left;
		out_window_height = client_rect.bottom - client_rect.top;
	}

	void window_get_center(int32_t& out_centerX, int32_t& out_centerY)
	{
		RECT window_rect = {};
		GetWindowRect(internal.hwnd, &window_rect);

		RECT client_rect = {};
		GetClientRect(internal.hwnd, &client_rect);

		client_rect.left = window_rect.left;
		client_rect.right += window_rect.left;
		client_rect.top = window_rect.top;
		client_rect.bottom += window_rect.top;

		out_centerX = client_rect.left + (client_rect.right - client_rect.left) / 2;
		out_centerY = client_rect.top + (client_rect.bottom - client_rect.top) / 2;
	}

	void window_reset_mouse_to_center()
	{
		int32_t window_centerX, window_centerY;
		window_get_center(window_centerX, window_centerY);
		SetCursorPos(window_centerX, window_centerY);
	}

	void window_set_capture_mouse(bool capture)
	{
		RECT window_rect = {};
		GetWindowRect(internal.hwnd, &window_rect);

		RECT client_rect = {};
		GetClientRect(internal.hwnd, &client_rect);

		client_rect.left = window_rect.left;
		client_rect.right += window_rect.left;
		client_rect.top = window_rect.top;
		client_rect.bottom += window_rect.top;

		ShowCursor(!capture);
		ClipCursor(capture ? &client_rect : nullptr);

		window_reset_mouse_to_center();

		input::set_mouse_capture(capture);
	}

	bool window_poll_events()
	{
		input::reset();
		input::update_mouse_pos();

		MSG msg = {};
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != NULL)
		{
			if (msg.message == WM_QUIT)
				return false;

			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		return true;
	}

	timer_t get_ticks()
	{
		LARGE_INTEGER counter = {};
		if (!QueryPerformanceCounter(&counter))
		{
			FATAL_ERROR("Platform", "Failed call to QueryPerformanceCounter");
		}

		return timer_t{ counter.QuadPart };
	}

	double get_elapsed_seconds(timer_t begin, timer_t end)
	{
		ASSERT(begin.val <= end.val);
		return (double)(end.val - begin.val) / (double)internal.perf_freq;
	}

	void window_create(int32_t desired_width, int32_t desired_height)
	{
		int32_t screen_width = GetSystemMetrics(SM_CXFULLSCREEN);
		int32_t screen_height = GetSystemMetrics(SM_CYFULLSCREEN);

		if (desired_width <= 0 || desired_width > screen_width ||
			desired_height <= 0 || desired_height > screen_height)
		{
			desired_width = 4 * screen_width / 5;
			desired_height = 4 * screen_height / 5;
		}

		WNDCLASSEXW window_class =
		{
			.cbSize = sizeof(window_class),
			.style = CS_HREDRAW | CS_VREDRAW,
			.lpfnWndProc = &window_proc,
			.hIcon = LoadIconW(NULL, L"APPICON"),
			.hCursor = NULL,
			.lpszClassName = L"WavefrontPathtracerWindowClass"
		};

		if (!RegisterClassExW(&window_class))
			FATAL_ERROR("Window", "Failed to register window class");

		RECT window_rect =
		{
			.left = 0,
			.top = 0,
			.right = desired_width,
			.bottom = desired_height
		};
		AdjustWindowRectEx(&window_rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

		int32_t window_width = window_rect.right - window_rect.left;
		int32_t window_height = window_rect.bottom - window_rect.top;

		int32_t windowX = glm::max(0, (screen_width - window_width) / 2);
		int32_t windowY = glm::max(0, (screen_height - window_height) / 2);
		
		internal.hwnd = CreateWindowExW(
			0, L"WavefrontPathtracerWindowClass", L"Wavefront Pathtracer", WS_OVERLAPPEDWINDOW,
			windowX, windowY, window_width, window_height,
			NULL, NULL, NULL, NULL
		);

		if (!internal.hwnd)
			FATAL_ERROR("Window", "Failed to create window");

		ShowWindow(internal.hwnd, TRUE);
	}

	void fatal_error(int32_t line, const char* error_msg)
	{
		MessageBoxA(NULL, error_msg, "Fatal Error", MB_OK);
		__debugbreak();
		ExitProcess(1);
	}

	bool show_message_box(const char* title, const char* message)
	{
		int32_t result = MessageBoxA(NULL, message, title, MB_RETRYCANCEL);

		switch (result)
		{
		case IDNO:
		case IDABORT:
		case IDCANCEL:
		case IDIGNORE:
			return false;
		case IDYES:
		case IDOK:
		case IDRETRY:
		case IDTRYAGAIN:
		case IDCONTINUE:
			return true;
		default:
			return false;
		}
	}

	static void parse_next_command_arg(string_t& cmd_line_cur, string_t& arg_str, string_t& param_str)
	{
		uint32_t arg_begin = string::find_char(cmd_line_cur, '-');
		uint32_t arg_end = string::find_char(cmd_line_cur, ' ');
		if (arg_end == STRING_NPOS || arg_begin >= arg_end)
			FATAL_ERROR("CommandLine", "Malformed command line arguments found: %s", cmd_line_cur);

		arg_str = string::make_view(cmd_line_cur, arg_begin, arg_end - arg_begin);
		cmd_line_cur = string::make_view(cmd_line_cur, arg_end + 1, cmd_line_cur.count - arg_end - 1);

		uint32_t param_begin = 0;
		uint32_t param_end = string::find_char(cmd_line_cur, ' ');
		if (param_end == STRING_NPOS)
			param_end = cmd_line_cur.count;

		if (param_begin >= param_end)
			FATAL_ERROR("CommandLine", "Malformed command line arguments found: %s", cmd_line_cur);

		param_str = string::make_view(cmd_line_cur, param_begin, param_end - param_begin);
		cmd_line_cur = string::make_view(cmd_line_cur, param_end + 1, cmd_line_cur.count - param_end - 1);
	}

	static void parse_cmd_line_args(const string_t& cmd_line, command_line_args_t& parsed_args)
	{
		if (cmd_line.count == 0)
			return;

		string_t cmd_line_cur = cmd_line;
		while (cmd_line_cur.count > 0)
		{
			string_t arg_str, param_str;
			parse_next_command_arg(cmd_line_cur, arg_str, param_str);
			char* param_end_ptr = param_str.buf + param_str.count;

			if (string::compare(arg_str, STRING_LITERAL("--width")))
			{
				parsed_args.window_width = strtol(param_str.buf, &param_end_ptr, 10);
			}
			else if (string::compare(arg_str, STRING_LITERAL("--height")))
			{
				parsed_args.window_height = strtol(param_str.buf, &param_end_ptr, 10);
			}
		}
	}

	static command_line_args_t get_default_cmd_line_args()
	{
		command_line_args_t default_args = {};
		default_args.window_width = 1920;
		default_args.window_height = 1080;

		return default_args;
	}

	command_line_args_t parse_command_line_args(memory_arena_t& arena, const wchar_t* cmd_line_ptr)
	{
		// Parse command line arguments
		command_line_args_t parsed_args = platform::get_default_cmd_line_args();

		ARENA_MEMORY_SCOPE(arena)
		{
			if (wcslen(cmd_line_ptr))
			{
				string_t cmd_line = ARENA_WIDE_TO_CHAR(arena, cmd_line_ptr);
				LOG_INFO("Command Line", "Passed arguments: %s", cmd_line.buf);
				platform::parse_cmd_line_args(cmd_line, parsed_args);
			}
			else
			{
				LOG_INFO("Command Line", "No command line arguments passed");
			}
		}

		return parsed_args;
	}

	void init()
	{
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
		
		// Only need to query the performance frequency once since it is determined at system boot
		LARGE_INTEGER freq = {};
		if (!QueryPerformanceFrequency(&freq))
		{
			FATAL_ERROR("Platform", "Failed call to QueryPerformanceFrequency");
		}
		internal.perf_freq = freq.QuadPart; // / 1000.0; // for milliseconds

		console_create();
	}

	void exit()
	{
		console_destroy();
	}
	
}

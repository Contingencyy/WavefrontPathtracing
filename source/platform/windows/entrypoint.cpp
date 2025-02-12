#include "pch.h"
#include "core/application.h"
#include "core/logger.h"
#include "core/input.h"
#include "core/string/string.h"

#include "platform/windows/windows_common.h"

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

#include "imgui/imgui_impl_win32.h"

static HWND s_hwnd;

static inline void create_console()
{
	bool result = AllocConsole();
	if (!result)
		FATAL_ERROR("Console", "Failed to allocate console");

	SetConsoleTitle("Wavefront Pathtracer Console");
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);
}

static inline void destroy_console()
{
	/*fclose(fileStdin);
	fclose(fileStdout);
	fclose(fileStderr);*/

	bool result = FreeConsole();
	if (!result)
		FATAL_ERROR("Console", "Failed to free console");
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT Message, WPARAM WParam, LPARAM LParam);
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

void get_window_client_area(int32_t& out_window_width, int32_t& out_window_height)
{
	RECT client_rect = {};
	GetClientRect(s_hwnd, &client_rect);

	out_window_width = client_rect.right - client_rect.left;
	out_window_height = client_rect.bottom - client_rect.top;
}

void get_window_center(int32_t& out_centerX, int32_t& out_centerY)
{
	RECT window_rect = {};
	GetWindowRect(s_hwnd, &window_rect);

	RECT client_rect = {};
	GetClientRect(s_hwnd, &client_rect);

	client_rect.left = window_rect.left;
	client_rect.right += window_rect.left;
	client_rect.top = window_rect.top;
	client_rect.bottom += window_rect.top;

	out_centerX = client_rect.left + (client_rect.right - client_rect.left) / 2;
	out_centerY = client_rect.top + (client_rect.bottom - client_rect.top) / 2;
}

void reset_mouse_position_to_center()
{
	int32_t window_centerX, window_centerY;
	get_window_center(window_centerX, window_centerY);
	SetCursorPos(window_centerX, window_centerY);
}

void set_window_capture_mouse(bool capture)
{
	RECT window_rect = {};
	GetWindowRect(s_hwnd, &window_rect);

	RECT client_rect = {};
	GetClientRect(s_hwnd, &client_rect);

	client_rect.left = window_rect.left;
	client_rect.right += window_rect.left;
	client_rect.top = window_rect.top;
	client_rect.bottom += window_rect.top;

	ShowCursor(!capture);
	ClipCursor(capture ? &client_rect : nullptr);

	reset_mouse_position_to_center();

	input::set_mouse_capture(capture);
}

bool poll_window_events()
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

void create_window(int32_t desired_width, int32_t desired_height)
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
	
	s_hwnd = CreateWindowExW(
		0, L"WavefrontPathtracerWindowClass", L"Wavefront Pathtracer", WS_OVERLAPPEDWINDOW,
		windowX, windowY, window_width, window_height,
		NULL, NULL, NULL, NULL
	);

	if (!s_hwnd)
		FATAL_ERROR("Window", "Failed to create window");

	ShowWindow(s_hwnd, TRUE);
}

void fatal_error_impl(int32_t line, const string_t& error_msg)
{
	ARENA_SCRATCH_SCOPE()
	{
		const char* error_msg_nullterm = string::make_nullterm(arena_scratch, error_msg);

		MessageBoxA(NULL, error_msg_nullterm, "Fatal Error", MB_OK);
		__debugbreak();
		ExitProcess(1);
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

int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int32_t nShowCmd)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	create_console();

	// Parse command line arguments
	command_line_args_t parsed_args = get_default_cmd_line_args();

	if (wcslen(lpCmdLine))
	{
		ARENA_SCRATCH_SCOPE()
		{
			string_t cmd_line = ARENA_WIDE_TO_CHAR(arena_scratch, lpCmdLine);
			LOG_INFO("Command Line", "Passed arguments: %s", cmd_line.buf);
			parse_cmd_line_args(cmd_line, parsed_args);
		}
	}

	while (!application::should_close())
	{
		application::init(parsed_args);
		application::run();
		application::exit();
	}

	destroy_console();

	return 0;
}
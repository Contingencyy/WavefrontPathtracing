#include "pch.h"
#include "core/application.h"
#include "core/logger.h"
#include "core/input.h"

#define WINDOWS_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#ifdef create_window
#undef create_window
#endif

#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

#ifdef LoadImage
#undef LoadImage
#endif

#ifdef OPAQUE
#undef OPAQUE
#endif

#ifdef TRANSPARENT
#undef TRANSPARENT
#endif

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

#include "imgui/imgui_impl_win32.h"

static HWND s_hwnd;

static inline void create_console()
{
	b8 result = AllocConsole();
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

	b8 result = FreeConsole();
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
		f32 wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
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

void get_window_client_area(i32& out_window_width, i32& out_window_height)
{
	RECT client_rect = {};
	GetClientRect(s_hwnd, &client_rect);

	out_window_width = client_rect.right - client_rect.left;
	out_window_height = client_rect.bottom - client_rect.top;
}

void get_window_center(i32& out_centerX, i32& out_centerY)
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
	i32 window_centerX, window_centerY;
	get_window_center(window_centerX, window_centerY);
	SetCursorPos(window_centerX, window_centerY);
}

void set_window_capture_mouse(b8 capture)
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

b8 poll_window_events()
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

static inline void create_window(i32 desired_client_width, i32 desired_client_height)
{
	i32 screen_width = GetSystemMetrics(SM_CXFULLSCREEN);
	i32 screen_height = GetSystemMetrics(SM_CYFULLSCREEN);

	if (desired_client_width <= 0 || desired_client_width > screen_width)
		desired_client_width = 4 * screen_width / 5;
	if (desired_client_height <= 0 || desired_client_height > screen_height)
		desired_client_height = 4 * screen_height / 5;

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
		.right = desired_client_width,
		.bottom = desired_client_height
	};
	AdjustWindowRectEx(&window_rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

	i32 window_width = window_rect.right - window_rect.left;
	i32 window_height = window_rect.bottom - window_rect.top;

	i32 windowX = glm::max(0, (screen_width - window_width) / 2);
	i32 windowY = glm::max(0, (screen_height - window_height) / 2);
	
	s_hwnd = CreateWindowExW(
		0, L"WavefrontPathtracerWindowClass", L"Wavefront Pathtracer", WS_OVERLAPPEDWINDOW,
		windowX, windowY, window_width, window_height,
		NULL, NULL, NULL, NULL
	);

	if (!s_hwnd)
		FATAL_ERROR("Window", "Failed to create window");

	ShowWindow(s_hwnd, TRUE);

	RECT client_rect;
	GetClientRect(s_hwnd, &client_rect);

	ASSERT(client_rect.right - client_rect.left == desired_client_width);
	ASSERT(client_rect.bottom - client_rect.top == desired_client_height);
}

void fatal_error_impl(i32 line, const char* error_msg)
{
	MessageBoxA(NULL, error_msg, "Fatal Error", MB_OK);
	__debugbreak();
	ExitProcess(1);
}

struct command_line_args_t
{
	i32 window_width = 0;
	i32 window_height = 0;
};

static command_line_args_t ParseCommandLineArgs(char* cmd_line)
{
	command_line_args_t cmd_args = {};
	char* arg_current = cmd_line;

	while (true)
	{
		const char* arg_begin = strchr(arg_current, '-');
		if (arg_begin == NULL)
			break;

		const char* arg_end = strchr(arg_begin, ' ');
		if (arg_end == NULL)
			FATAL_ERROR("CommandLine", "Malformed command line arguments found: %s", cmd_line);

		const char* param_begin = arg_end + 1;
		if (param_begin == NULL)
			FATAL_ERROR("CommandLine", "Malformed command line arguments found: %s", cmd_line);

		const char* param_end = strchr(param_begin, ' ');
		if (param_end == NULL)
			param_end = strchr(param_begin, '\0');

		char arg_str[32];
		strncpy_s(arg_str, arg_begin, arg_end - arg_begin);

		if (strcmp(arg_str, "--width") == 0)
		{
			cmd_args.window_width = strtol(param_begin, &arg_current, 10);
		}
		else if (strcmp(arg_str, "--height") == 0)
		{
			cmd_args.window_height = strtol(param_begin, &arg_current, 10);
		}
	}

	return cmd_args;
}

int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ i32 nShowCmd)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	create_console();

	char cmd_line[512];
	wcstombs(cmd_line, lpCmdLine, ARRAY_SIZE(cmd_line));
	command_line_args_t parsed_cmd_args = ParseCommandLineArgs(cmd_line);

	LOG_INFO("application", "Started with arguments: %s", cmd_line);
	
	create_window(parsed_cmd_args.window_width, parsed_cmd_args.window_height);

	while (!application::should_close())
	{
		application::init();
		application::run();
		application::exit();
	}

	destroy_console();

	return 0;
}
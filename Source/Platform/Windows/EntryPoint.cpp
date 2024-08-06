#include "Pch.h"
#include "Core/Application.h"
#include "Core/Logger.h"
#include "Core/Input.h"

#define WINDOWS_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#ifdef CreateWindow
#undef CreateWindow
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

static HWND s_HWND;

static inline void CreateConsole()
{
	b8 Result = AllocConsole();
	if (!Result)
		FATAL_ERROR("Console", "Failed to allocate console");

	SetConsoleTitle("Wavefront Pathtracer Console");
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);
}

static inline void DestroyConsole()
{
	/*fclose(fileStdin);
	fclose(fileStdout);
	fclose(fileStderr);*/

	b8 Result = FreeConsole();
	if (!Result)
		FATAL_ERROR("Console", "Failed to free console");
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT Message, WPARAM WParam, LPARAM LParam);
LRESULT WINAPI WindowProc(HWND hwnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, Message, WParam, LParam))
		return true;

	switch (Message)
	{
	case WM_SIZE:
	case WM_SIZING:
	{
		// TODO: Handle resizing swap chain and everything else that needs to happen when the window is resized
	} break;
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	{
		Input::OnPlatformKeyButtonStateChanged(WParam, true);
	} break;
	case WM_SYSKEYUP:
	case WM_KEYUP:
	{
		Input::OnPlatformKeyButtonStateChanged(WParam, false);
	} break;
	case WM_LBUTTONUP:
	{
		Input::OnPlatformKeyButtonStateChanged(VK_LBUTTON, false);
	} break;
	case WM_RBUTTONUP:
	{
		Input::OnPlatformKeyButtonStateChanged(VK_RBUTTON, false);
	} break;
	case WM_MOUSEWHEEL:
	{
		f32 wheelDelta = GET_WHEEL_DELTA_WPARAM(WParam);
		Input::OnMouseWheelScrolled(wheelDelta);
	} break;
	case WM_SETFOCUS:
	{
		Input::SetWindowFocus(true);
	} break;
	case WM_KILLFOCUS:
	{
		Input::SetWindowFocus(false);
	} break;
	case WM_DESTROY:
	{
		PostQuitMessage(0);
	} break;
	default:
	{
		return DefWindowProcW(hwnd, Message, WParam, LParam);
	} break;
	}

	return 0;
}

void GetWindowClientArea(i32& WindowWidth, i32& WindowHeight)
{
	RECT clientRect = {};
	GetClientRect(s_HWND, &clientRect);

	WindowWidth = clientRect.right - clientRect.left;
	WindowHeight = clientRect.bottom - clientRect.top;
}

void GetWindowCenter(i32& windowCenterX, i32& windowCenterY)
{
	RECT windowRect = {};
	GetWindowRect(s_HWND, &windowRect);

	RECT clientRect = {};
	GetClientRect(s_HWND, &clientRect);

	clientRect.left = windowRect.left;
	clientRect.right += windowRect.left;
	clientRect.top = windowRect.top;
	clientRect.bottom += windowRect.top;

	windowCenterX = clientRect.left + (clientRect.right - clientRect.left) / 2;
	windowCenterY = clientRect.top + (clientRect.bottom - clientRect.top) / 2;
}

void ResetMousePositionToCenter()
{
	i32 windowCenterX, windowCenterY;
	GetWindowCenter(windowCenterX, windowCenterY);
	SetCursorPos(windowCenterX, windowCenterY);
}

void SetWindowCaptureMouse(b8 bCapture)
{
	RECT windowRect = {};
	GetWindowRect(s_HWND, &windowRect);

	RECT clientRect = {};
	GetClientRect(s_HWND, &clientRect);

	clientRect.left = windowRect.left;
	clientRect.right += windowRect.left;
	clientRect.top = windowRect.top;
	clientRect.bottom += windowRect.top;

	ShowCursor(!bCapture);
	ClipCursor(bCapture ? &clientRect : nullptr);
	ResetMousePositionToCenter();

	Input::SetMouseCapture(bCapture);
}

b8 PollWindowEvents()
{
	Input::Reset();
	Input::UpdateMousePos();

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

static inline void CreateWindow(i32 windowWidth, i32 windowHeight)
{
	i32 screenWidth = GetSystemMetrics(SM_CXFULLSCREEN);
	i32 screenHeight = GetSystemMetrics(SM_CYFULLSCREEN);

	if (windowWidth <= 0 || windowWidth > screenWidth)
		windowWidth = 4 * screenWidth / 5;
	if (windowHeight <= 0 || windowHeight > screenHeight)
		windowHeight = 4 * screenHeight / 5;

	WNDCLASSEXW windowClass =
	{
		.cbSize = sizeof(windowClass),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = &WindowProc,
		.hIcon = LoadIconW(NULL, L"APPICON"),
		.hCursor = NULL,
		.lpszClassName = L"WavefrontPathtracerWindowClass"
	};

	if (!RegisterClassExW(&windowClass))
		FATAL_ERROR("Window", "Failed to register window class");

	RECT windowRect =
	{
		.left = 0,
		.top = 0,
		.right = windowWidth,
		.bottom = windowHeight
	};

	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	s_HWND = CreateWindowExW(
		0, L"WavefrontPathtracerWindowClass", L"Wavefront Pathtracer", WS_OVERLAPPEDWINDOW,
		screenWidth - windowRect.right - 32, screenHeight - windowRect.bottom - 32, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
		NULL, NULL, NULL, NULL
	);
	
	if (!s_HWND)
		FATAL_ERROR("Window", "Failed to create window");

	ShowWindow(s_HWND, TRUE);
}

void FatalErrorImpl(i32 line, const char* ErrorMessage)
{
	MessageBoxA(NULL, ErrorMessage, "Fatal Error", MB_OK);
	__debugbreak();
	ExitProcess(1);
}

struct CommandLineArgs
{
	i32 WindowWidth = 0;
	i32 WindowHeight = 0;
};

static CommandLineArgs ParseCommandLineArgs(char* CmdLine)
{
	CommandLineArgs CmdArgs = {};
	char* ArgCurrent = CmdLine;

	while (true)
	{
		const char* ArgBegin = strchr(ArgCurrent, '-');
		if (ArgBegin == NULL)
			break;

		const char* ArgEnd = strchr(ArgBegin, ' ');
		if (ArgEnd == NULL)
			FATAL_ERROR("CommandLine", "Malformed command line arguments found: %s", CmdLine);

		const char* ParamBegin = ArgEnd + 1;
		if (ParamBegin == NULL)
			FATAL_ERROR("CommandLine", "Malformed command line arguments found: %s", CmdLine);

		const char* ParamEnd = strchr(ParamBegin, ' ');
		if (ParamEnd == NULL)
			ParamEnd = strchr(ParamBegin, '\0');

		char ArgStr[32];
		strncpy_s(ArgStr, ArgBegin, ArgEnd - ArgBegin);

		if (strcmp(ArgStr, "--width") == 0)
		{
			CmdArgs.WindowWidth = strtol(ParamBegin, &ArgCurrent, 10);
		}
		else if (strcmp(ArgStr, "--height") == 0)
		{
			CmdArgs.WindowHeight = strtol(ParamBegin, &ArgCurrent, 10);
		}
	}

	return CmdArgs;
}

int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ i32 nShowCmd)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	CreateConsole();

	char CmdLine[512];
	wcstombs(CmdLine, lpCmdLine, ARRAY_SIZE(CmdLine));
	CommandLineArgs CmdArgs = ParseCommandLineArgs(CmdLine);

	LOG_INFO("Application", "Started with arguments: %s", CmdLine);
	
	CreateWindow(CmdArgs.WindowWidth, CmdArgs.WindowHeight);

	while (!Application::ShouldClose())
	{
		Application::Init();
		Application::Run();
		Application::Exit();
	}

	DestroyConsole();

	return 0;
}
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
	BOOL result = AllocConsole();
	if (!result)
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

	BOOL result = FreeConsole();
	if (!result)
		FATAL_ERROR("Console", "Failed to free console");
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam))
		return true;

	switch (message)
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
		Input::OnPlatformKeyButtonStateChanged(wparam, true);
	} break;
	case WM_SYSKEYUP:
	case WM_KEYUP:
	{
		Input::OnPlatformKeyButtonStateChanged(wparam, false);
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
		float wheelDelta = GET_WHEEL_DELTA_WPARAM(wparam);
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
		return DefWindowProcW(hwnd, message, wparam, lparam);
	} break;
	}

	return 0;
}

void GetWindowClientArea(int32_t& windowWidth, int32_t& windowHeight)
{
	RECT clientRect = {};
	GetClientRect(s_HWND, &clientRect);

	windowWidth = clientRect.right - clientRect.left;
	windowHeight = clientRect.bottom - clientRect.top;
}

void GetWindowCenter(int32_t& windowCenterX, int32_t& windowCenterY)
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
	int32_t windowCenterX, windowCenterY;
	GetWindowCenter(windowCenterX, windowCenterY);
	SetCursorPos(windowCenterX, windowCenterY);
}

void SetWindowCaptureMouse(bool capture)
{
	RECT windowRect = {};
	GetWindowRect(s_HWND, &windowRect);

	RECT clientRect = {};
	GetClientRect(s_HWND, &clientRect);

	clientRect.left = windowRect.left;
	clientRect.right += windowRect.left;
	clientRect.top = windowRect.top;
	clientRect.bottom += windowRect.top;

	ShowCursor(!capture);
	ClipCursor(capture ? &clientRect : nullptr);
	ResetMousePositionToCenter();

	Input::SetMouseCapture(capture);
}

bool PollWindowEvents()
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

static inline void CreateWindow(int32_t windowWidth, int32_t windowHeight)
{
	int32_t screenWidth = GetSystemMetrics(SM_CXFULLSCREEN);
	int32_t screenHeight = GetSystemMetrics(SM_CYFULLSCREEN);

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

void FatalErrorImpl(int line, const std::string& file, const std::string& sender, const std::string& errorMsg)
{
	MessageBoxA(NULL, errorMsg.c_str(), "Fatal Error", MB_OK);
	__debugbreak;
	ExitProcess(1);
}

struct CommandLineArgs
{
	int32_t windowWidth = 0;
	int32_t windowHeight = 0;
};

static std::string WStringToString(const std::wstring_view& wstring)
{
	std::string result;
	result.resize(wstring.size());
	int conversionCode = WideCharToMultiByte(CP_UTF8, 0, wstring.data(), wstring.size(), result.data(), result.size(), NULL, nullptr);

	return result;
}

template<typename int32_t>
void ParseCommandLineArg(const std::string_view& cmdLine, const std::string_view& argStr, int32_t& parsedValue)
{
	size_t argParamBegin = cmdLine.find(argStr, 0) + argStr.size() + 1;
	size_t argParamEnd = std::min(cmdLine.find(" ", argParamBegin), cmdLine.size());

	parsedValue = std::stoi(std::string(cmdLine.substr(argParamBegin, argParamEnd - argParamBegin)));
}

static CommandLineArgs ParseCommandLineArgs(const std::string_view& cmdLine)
{
	CommandLineArgs cmdArgs = {};

	const std::string widthStr = "--width";
	ParseCommandLineArg(cmdLine, widthStr, cmdArgs.windowWidth);

	const std::string heightStr = "--height";
	ParseCommandLineArg(cmdLine, heightStr, cmdArgs.windowHeight);

	return cmdArgs;
}

int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	CreateConsole();

	std::string cmdLineStr = WStringToString(lpCmdLine);
	CommandLineArgs cmdArgs = ParseCommandLineArgs(cmdLineStr);

	LOG_INFO("Application", "Started with arguments: {}", cmdLineStr);
	
	CreateWindow(cmdArgs.windowWidth, cmdArgs.windowHeight);

	while (!Application::ShouldClose())
	{
		Application::Init();
		Application::Run();
		Application::Exit();
	}

	DestroyConsole();

	return 0;
}
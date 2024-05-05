#include "Pch.h"
#include "Application.h"
#include "Logger.h"

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

LRESULT WINAPI WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
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

bool PollWindowEvents()
{
	MSG msg = {};
	while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != NULL)
	{
		if (msg.message == WM_QUIT)
			return false;

		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return true;
}

static inline void CreateWindow()
{
	int32_t screenWidth = GetSystemMetrics(SM_CXFULLSCREEN);
	int32_t screenHeight = GetSystemMetrics(SM_CYFULLSCREEN);

	int32_t windowWidth = 4 * screenWidth / 5;
	int32_t windowHeight = 4 * screenHeight / 5;

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

	HWND window = CreateWindowExW(
		0, L"WavefrontPathtracerWindowClass", L"Wavefront Pathtracer", WS_OVERLAPPEDWINDOW,
		0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
		NULL, NULL, NULL, NULL
	);
	
	if (!window)
		FATAL_ERROR("Window", "Failed to create window");

	ShowWindow(window, TRUE);
}

void FatalErrorImpl(int line, const std::string& file, const std::string& sender, const std::string& formattedMessage)
{
	std::string errorMessage = std::format("Fatal Error Occured\n[{}] {}\nFile: {}\nLine: {}\n", sender, formattedMessage, file, line);

	MessageBoxA(NULL, errorMessage.c_str(), "Fatal Error", MB_OK);
	__debugbreak;
	ExitProcess(1);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prevInstance, PWSTR cmdLine, int cmdShow)
{
	CreateConsole();
	CreateWindow();

	while (!Application::ShouldClose())
	{
		Application::Init();
		Application::Run();
		Application::Exit();
	}

	DestroyConsole();

	return 0;
}
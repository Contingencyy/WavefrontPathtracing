#include "Pch.h"
#include "Core/Input.h"

#define WINDOWS_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

void ResetMousePositionToCenter();
void GetWindowCenter(i32& windowCenterX, i32& windowCenterY);

namespace Input
{

	static std::unordered_map<WPARAM, KeyCode> WParamToKeyCode
	{
		{ VK_LBUTTON, KeyCode_LeftMouse }, { VK_RBUTTON, KeyCode_RightMouse },
		{ 0x57, KeyCode_W }, { 0x53, KeyCode_S }, { 0x41, KeyCode_A }, { 0x44, KeyCode_D },
		{ VK_SPACE, KeyCode_Space }, { VK_CONTROL, KeyCode_LeftCtrl } , { VK_SHIFT, KeyCode_LeftShift }
	};

	static b8 KeyStates[KeyCode_NumKeys];
	static b8 bCapturingMouse = false;
	static b8 bWindowFocus = true;

	static i32 PrevMouseX = 0;
	static i32 PrevMouseY = 0;
	static i32 CurrMouseX = 0;
	static i32 CurrMouseY = 0;

	static f32 MouseWheelDelta = 0.0f;

	void OnPlatformKeyButtonStateChanged(u64 PlatformCode, b8 bPressed)
	{
		if (WParamToKeyCode.find(PlatformCode) != WParamToKeyCode.end())
			KeyStates[WParamToKeyCode.at(PlatformCode)] = bPressed;
	}

	void OnMouseWheelScrolled(f32 WheelDelta)
	{
		MouseWheelDelta += WheelDelta;
	}

	void UpdateMousePos()
	{
		PrevMouseX = CurrMouseX;
		PrevMouseY = CurrMouseY;

		POINT mousePos = {};
		GetCursorPos(&mousePos);

		CurrMouseX = mousePos.x;
		CurrMouseY = mousePos.y;

		if (bCapturingMouse)
		{
			// If we are capturing the mouse, we reset the mouse Position to the Center of the window
			// However, this would mess with our mouse movement, so we need to move the mouse over artificially while preserving the delta
			i32 centerX, centerY;
			GetWindowCenter(centerX, centerY);

			i32 deltaX = centerX - CurrMouseX;
			i32 deltaY = centerY - CurrMouseY;
			CurrMouseX += deltaX;
			CurrMouseY += deltaY;
			PrevMouseX += deltaX;
			PrevMouseY += deltaY;

			ResetMousePositionToCenter();
		}
	}

	b8 IsKeyPressed(KeyCode KeyCode)
	{
		return KeyStates[KeyCode];
	}

	f32 GetAxis1D(KeyCode AxisPos, KeyCode AxisNeg)
	{
		return static_cast<f32>(KeyStates[AxisPos]) + (-static_cast<f32>(KeyStates[AxisNeg]));
	}

	f32 GetMouseRelX()
	{
		return static_cast<f32>(CurrMouseX - PrevMouseX);
	}

	f32 GetMouseRelY()
	{
		return static_cast<f32>(CurrMouseY - PrevMouseY);
	}

	f32 GetMouseScrollRelY()
	{
		return MouseWheelDelta;
	}

	void SetMouseCapture(b8 bCapture)
	{
		bCapturingMouse = bCapture;

		// Reset the mouse positions so there is no instant teleportation once mouse capture starts
		if (bCapturingMouse)
		{
			POINT mousePos = {};
			GetCursorPos(&mousePos);

			PrevMouseX = CurrMouseX = mousePos.x;
			PrevMouseY = CurrMouseY = mousePos.y;
		}
	}

	b8 IsMouseCaptured()
	{
		return bCapturingMouse && bWindowFocus;
	}

	void SetWindowFocus(b8 bFocus)
	{
		bWindowFocus = bFocus;
	}

	void Reset()
	{
		MouseWheelDelta = 0.0f;
	}

}

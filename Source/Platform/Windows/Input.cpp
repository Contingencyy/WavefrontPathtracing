#include "Pch.h"
#include "Core/Input.h"

#define WINDOWS_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

void ResetMousePositionToCenter();
void GetWindowCenter(int32_t& windowCenterX, int32_t& windowCenterY);

namespace Input
{

	static std::unordered_map<WPARAM, KeyCode> wparamToKeyCode
	{
		{ VK_LBUTTON, KeyCode_LeftMouse }, { VK_RBUTTON, KeyCode_RightMouse },
		{ 0x57, KeyCode_W }, { 0x53, KeyCode_S }, { 0x41, KeyCode_A }, { 0x44, KeyCode_D },
		{ VK_SPACE, KeyCode_Space }, { VK_CONTROL, KeyCode_LeftCtrl } , { VK_SHIFT, KeyCode_LeftShift }
	};

	static std::vector<bool> keyStates = std::vector<bool>(KeyCode_NumKeys);
	static bool capturingMouse = false;
	static bool windowFocus = true;

	static int32_t prevMouseX = 0;
	static int32_t prevMouseY = 0;
	static int32_t currMouseX = 0;
	static int32_t currMouseY = 0;

	static float mouseWheelDelta = 0.0f;

	void OnPlatformKeyButtonStateChanged(uint64_t platformCode, bool pressed)
	{
		if (wparamToKeyCode.find(platformCode) != wparamToKeyCode.end())
			keyStates.at(wparamToKeyCode.at(platformCode)) = pressed;
	}

	void OnMouseWheelScrolled(float wheelDelta)
	{
		mouseWheelDelta += wheelDelta;
	}

	void UpdateMousePos()
	{
		prevMouseX = currMouseX;
		prevMouseY = currMouseY;

		POINT mousePos = {};
		GetCursorPos(&mousePos);

		currMouseX = mousePos.x;
		currMouseY = mousePos.y;

		if (capturingMouse)
		{
			// If we are capturing the mouse, we reset the mouse position to the center of the window
			// However, this would mess with our mouse movement, so we need to move the mouse over artificially while preserving the delta
			int32_t centerX, centerY;
			GetWindowCenter(centerX, centerY);

			int32_t deltaX = centerX - currMouseX;
			int32_t deltaY = centerY - currMouseY;
			currMouseX += deltaX;
			currMouseY += deltaY;
			prevMouseX += deltaX;
			prevMouseY += deltaY;

			ResetMousePositionToCenter();
		}
	}

	bool IsKeyPressed(KeyCode keyCode)
	{
		return keyStates.at(keyCode);
	}

	float GetAxis1D(KeyCode axisPos, KeyCode axisNeg)
	{
		return static_cast<float>(keyStates[axisPos]) + (-static_cast<float>(keyStates[axisNeg]));
	}

	float GetMouseRelX()
	{
		return static_cast<float>(currMouseX - prevMouseX);
	}

	float GetMouseRelY()
	{
		return static_cast<float>(currMouseY - prevMouseY);
	}

	float GetMouseScrollRelY()
	{
		return mouseWheelDelta;
	}

	void SetMouseCapture(bool capture)
	{
		capturingMouse = capture;

		// Reset the mouse positions so there is no instant teleportation once mouse capture starts
		if (capturingMouse)
		{
			POINT mousePos = {};
			GetCursorPos(&mousePos);

			prevMouseX = currMouseX = mousePos.x;
			prevMouseY = currMouseY = mousePos.y;
		}
	}

	bool IsMouseCaptured()
	{
		return capturingMouse && windowFocus;
	}

	void SetWindowFocus(bool focus)
	{
		windowFocus = focus;
	}

	void Reset()
	{
		mouseWheelDelta = 0.0f;
	}

}

#pragma once

namespace Input
{

	enum KeyCode
	{
		KeyCode_LeftMouse,
		KeyCode_RightMouse,
		KeyCode_W,
		KeyCode_A,
		KeyCode_S,
		KeyCode_D,
		KeyCode_Space,
		KeyCode_LeftCtrl,
		KeyCode_LeftShift,
		KeyCode_NumKeys,
		KeyCode_Invalid
	};

	void OnPlatformKeyButtonStateChanged(uint64_t platformCode, bool pressed);
	void OnMouseWheelScrolled(float wheelDelta);
	void UpdateMousePos();

	bool IsKeyPressed(KeyCode keyCode);
	float GetAxis1D(KeyCode axisPos, KeyCode axisNeg);

	float GetMouseRelX();
	float GetMouseRelY();
	float GetMouseScrollRelY();

	void SetMouseCapture(bool capture);
	bool IsMouseCaptured();
	void SetWindowFocus(bool focus);

	void Reset();

}

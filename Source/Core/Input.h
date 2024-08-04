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

	void OnPlatformKeyButtonStateChanged(u64 PlatformCode, b8 bPressed);
	void OnMouseWheelScrolled(f32 WheelDelta);
	void UpdateMousePos();

	b8 IsKeyPressed(KeyCode KeyCode);
	f32 GetAxis1D(KeyCode AxisPos, KeyCode AxisNeg);

	f32 GetMouseRelX();
	f32 GetMouseRelY();
	f32 GetMouseScrollRelY();

	void SetMouseCapture(b8 bCapture);
	b8 IsMouseCaptured();
	void SetWindowFocus(b8 bFocus);

	void Reset();

}

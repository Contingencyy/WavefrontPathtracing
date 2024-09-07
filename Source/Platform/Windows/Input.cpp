#include "pch.h"
#include "core/input.h"
#include "core/containers/hashmap.h"

#define WINDOWS_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

void reset_mouse_position_to_center();
void get_window_center(i32& out_centerX, i32& out_centerY);

namespace input
{

	static keycode_t wparam_to_keycode(WPARAM wparam)
	{
		switch (wparam)
		{
		case VK_LBUTTON: return KeyCode_LeftMouse;
		case VK_RBUTTON: return KeyCode_RightMouse;
		case 0x57:		 return KeyCode_W;
		case 0x53:		 return KeyCode_S;
		case 0x41:		 return KeyCode_A;
		case 0x44:		 return KeyCode_D;
		case VK_SPACE:	 return KeyCode_Space;
		case VK_CONTROL: return KeyCode_LeftCtrl;
		case VK_SHIFT:	 return KeyCode_LeftShift;
		default:		 return KeyCode_NumKeys;
		}
	}

	static b8 s_keystates[KeyCode_NumKeys];
	static b8 s_capturing_mouse = false;
	static b8 s_window_focused = true;

	static i32 s_prev_mouseX = 0;
	static i32 s_prev_mouseY = 0;
	static i32 s_curr_mouseX = 0;
	static i32 s_curr_mouseY = 0;

	static f32 s_mousewheel_delta = 0.0f;

	void on_platform_key_button_state_changed(u64 platformcode, b8 pressed)
	{
		if (wparam_to_keycode(platformcode) != KeyCode_NumKeys)
			s_keystates[wparam_to_keycode(platformcode)] = pressed;
	}

	void on_mousewheel_scrolled(f32 wheel_delta)
	{
		s_mousewheel_delta += wheel_delta;
	}

	void update_mouse_pos()
	{
		s_prev_mouseX = s_curr_mouseX;
		s_prev_mouseY = s_curr_mouseY;

		POINT mouse_pos = {};
		GetCursorPos(&mouse_pos);

		s_curr_mouseX = mouse_pos.x;
		s_curr_mouseY = mouse_pos.y;

		if (s_capturing_mouse)
		{
			// If we are capturing the mouse, we reset the mouse position to the center of the window
			// However, this would mess with our mouse movement, so we need to move the mouse over artificially while preserving the delta
			i32 centerX, centerY;
			get_window_center(centerX, centerY);

			i32 deltaX = centerX - s_curr_mouseX;
			i32 deltaY = centerY - s_curr_mouseY;
			s_curr_mouseX += deltaX;
			s_curr_mouseY += deltaY;
			s_prev_mouseX += deltaX;
			s_prev_mouseY += deltaY;

			reset_mouse_position_to_center();
		}
	}

	b8 is_key_pressed(keycode_t keycode)
	{
		return s_keystates[keycode];
	}

	f32 get_axis_1D(keycode_t axis_pos, keycode_t axis_neg)
	{
		return static_cast<f32>(s_keystates[axis_pos]) + (-static_cast<f32>(s_keystates[axis_neg]));
	}

	f32 get_mouse_relX()
	{
		return static_cast<f32>(s_curr_mouseX - s_prev_mouseX);
	}

	f32 get_mouse_relY()
	{
		return static_cast<f32>(s_curr_mouseY - s_prev_mouseY);
	}

	f32 get_mouse_scroll_relY()
	{
		return s_mousewheel_delta;
	}

	void set_mouse_capture(b8 bCapture)
	{
		s_capturing_mouse = bCapture;

		// reset the mouse positions so there is no instant teleportation once mouse capture starts
		if (s_capturing_mouse)
		{
			POINT mousePos = {};
			GetCursorPos(&mousePos);

			s_prev_mouseX = s_curr_mouseX = mousePos.x;
			s_prev_mouseY = s_curr_mouseY = mousePos.y;
		}
	}

	b8 is_mouse_captured()
	{
		return s_capturing_mouse && s_window_focused;
	}

	void set_window_focus(b8 bFocus)
	{
		s_window_focused = bFocus;
	}

	void reset()
	{
		s_mousewheel_delta = 0.0f;
	}

}

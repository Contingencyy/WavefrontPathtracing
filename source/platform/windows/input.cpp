#include "pch.h"
#include "core/input.h"

#include "platform/platform.h"
#include "platform/windows/windows_common.h"

namespace input
{

	static KEYCODE wparam_to_keycode(WPARAM wparam)
	{
		switch (wparam)
		{
		case VK_LBUTTON: return KEYCODE_LEFT_MOUSE;
		case VK_RBUTTON: return KEYCODE_RIGHT_MOUSE;
		case 0x57:		 return KEYCODE_W;
		case 0x53:		 return KEYCODE_S;
		case 0x41:		 return KEYCODE_A;
		case 0x44:		 return KEYCODE_D;
		case VK_SPACE:	 return KEYCODE_SPACE;
		case VK_CONTROL: return KEYCODE_LEFT_CTRL;
		case VK_SHIFT:	 return KEYCODE_LEFT_SHIFT;
		default:		 return KEYCODE_NUM_KEYS;
		}
	}

	static bool s_keystates[KEYCODE_NUM_KEYS];
	static bool s_capturing_mouse = false;
	static bool s_window_focused = true;

	static int32_t s_prev_mouseX = 0;
	static int32_t s_prev_mouseY = 0;
	static int32_t s_curr_mouseX = 0;
	static int32_t s_curr_mouseY = 0;

	static float s_mousewheel_delta = 0.0f;

	void on_platform_key_button_state_changed(uint64_t platformcode, bool pressed)
	{
		if (wparam_to_keycode(platformcode) != KEYCODE_NUM_KEYS)
			s_keystates[wparam_to_keycode(platformcode)] = pressed;
	}

	void on_mousewheel_scrolled(float wheel_delta)
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

		if (s_capturing_mouse && s_window_focused)
		{
			// If we are capturing the mouse, we reset the mouse position to the center of the window
			// However, this would mess with our mouse movement, so we need to move the mouse over artificially while preserving the delta
			int32_t centerX, centerY;
			platform::window_get_center(centerX, centerY);

			int32_t deltaX = centerX - s_curr_mouseX;
			int32_t deltaY = centerY - s_curr_mouseY;
			s_curr_mouseX += deltaX;
			s_curr_mouseY += deltaY;
			s_prev_mouseX += deltaX;
			s_prev_mouseY += deltaY;

			platform::window_reset_mouse_to_center();
		}
	}

	bool is_key_pressed(KEYCODE keycode)
	{
		return s_keystates[keycode];
	}

	float get_axis_1d(KEYCODE axis_pos, KEYCODE axis_neg)
	{
		return (float)(s_keystates[axis_pos]) + (-(float)(s_keystates[axis_neg]));
	}

	float get_mouse_relx()
	{
		return (float)(s_curr_mouseX - s_prev_mouseX);
	}

	float get_mouse_rely()
	{
		return (float)(s_curr_mouseY - s_prev_mouseY);
	}

	float get_mouse_scroll_rely()
	{
		return s_mousewheel_delta;
	}

	void set_mouse_capture(bool bCapture)
	{
		s_capturing_mouse = bCapture;

		// reset the mouse positions so there is no instant teleportation once mouse capture starts
		if (s_capturing_mouse && s_window_focused)
		{
			POINT mousePos = {};
			GetCursorPos(&mousePos);

			s_prev_mouseX = s_curr_mouseX = mousePos.x;
			s_prev_mouseY = s_curr_mouseY = mousePos.y;
		}
	}

	bool is_mouse_captured()
	{
		return s_capturing_mouse;
	}

	void set_window_focus(bool bFocus)
	{
		s_window_focused = bFocus;
	}

	bool is_window_focused()
	{
		return s_window_focused;
	}

	void reset()
	{
		s_mousewheel_delta = 0.0f;
	}

}

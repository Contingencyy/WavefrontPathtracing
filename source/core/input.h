#pragma once

namespace input
{

	enum keycode_t
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

	void on_platform_key_button_state_changed(uint64_t platformcode, bool pressed);
	void on_mousewheel_scrolled(float wheel_delta);
	void update_mouse_pos();

	bool is_key_pressed(keycode_t keycode);
	float get_axis_1D(keycode_t axis_pos, keycode_t axis_neg);

	float get_mouse_relX();
	float get_mouse_relY();
	float get_mouse_scroll_relY();

	void set_mouse_capture(bool capture);
	bool is_mouse_captured();
	void set_window_focus(bool focus);

	void reset();

}

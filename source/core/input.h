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

	void on_platform_key_button_state_changed(u64 platformcode, b8 pressed);
	void on_mousewheel_scrolled(f32 wheel_delta);
	void update_mouse_pos();

	b8 is_key_pressed(keycode_t keycode);
	f32 get_axis_1D(keycode_t axis_pos, keycode_t axis_neg);

	f32 get_mouse_relX();
	f32 get_mouse_relY();
	f32 get_mouse_scroll_relY();

	void set_mouse_capture(b8 capture);
	b8 is_mouse_captured();
	void set_window_focus(b8 focus);

	void reset();

}

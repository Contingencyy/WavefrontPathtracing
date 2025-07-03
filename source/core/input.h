#pragma once

namespace input
{

	enum KEYCODE : uint32_t
	{
		KEYCODE_LEFT_MOUSE,
		KEYCODE_RIGHT_MOUSE,
		KEYCODE_W,
		KEYCODE_A,
		KEYCODE_S,
		KEYCODE_D,
		KEYCODE_SPACE,
		KEYCODE_LEFT_CTRL,
		KEYCODE_LEFT_SHIFT,
		
		KEYCODE_NUM_KEYS,
		KEYCODE_INVALID = 0xFFFFFFFFu
	};

	void on_platform_key_button_state_changed(uint64_t platformcode, bool pressed);
	void on_mousewheel_scrolled(float wheel_delta);
	void update_mouse_pos();

	bool is_key_pressed(KEYCODE keycode);
	float get_axis_1d(KEYCODE axis_pos, KEYCODE axis_neg);

	float get_mouse_relx();
	float get_mouse_rely();
	float get_mouse_scroll_rely();

	void set_mouse_capture(bool capture);
	bool is_mouse_captured();
	void set_window_focus(bool focus);

	void reset();

}

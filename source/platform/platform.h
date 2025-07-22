#pragma once
#include "core/common.h"
#include "core/api_types.h"
#include "core/application.h"

struct memory_arena_t;

namespace platform
{

    command_line_args_t parse_command_line_args(memory_arena_t& arena, const wchar_t* cmd_line_ptr);
    void init();
    void exit();
    
    void window_create(int32_t desired_width, int32_t desired_height);
    void window_get_client_area(int32_t& out_window_width, int32_t& out_window_height);
    void window_set_capture_mouse(bool capture);
    bool window_poll_events();
    void window_reset_mouse_to_center();
    void window_get_center(int32_t& out_centerX, int32_t& out_centerY);

    timer_t get_ticks();
    double get_elapsed_seconds(timer_t begin, timer_t end);

    void fatal_error(int32_t line, const char* error_msg);
    bool show_message_box(const char* title, const char* message);
    
}

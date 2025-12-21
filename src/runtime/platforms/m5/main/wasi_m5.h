#pragma once

#include "wasm_export.h"

#ifdef __cplusplus
extern "C"
{
#endif
    // TODO: currently, return void cannot be registered to WAMR.

    int m5_setup(wasm_exec_env_t exec_env);

    // ====================
    // Display
    // ====================
    // display size
    int32_t m5_lcd_width(wasm_exec_env_t exec_env);
    int32_t m5_lcd_height(wasm_exec_env_t exec_env);

    // display operations
    int m5_lcd_set_rotation(wasm_exec_env_t exec_env, int32_t r);
    int m5_lcd_fill_screen(wasm_exec_env_t exec_env, uint32_t rgb565);
    int m5_lcd_draw_pixel(wasm_exec_env_t exec_env, int32_t x, int32_t y, uint32_t rgb565);
    int m5_lcd_draw_line(wasm_exec_env_t exec_env, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t rgb565);
    int m5_lcd_draw_rect(wasm_exec_env_t exec_env, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb565);
    int m5_lcd_fill_rect(wasm_exec_env_t exec_env, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb565);
    int m5_lcd_draw_circle(wasm_exec_env_t exec_env, int32_t x0, int32_t y0, int32_t r, uint32_t rgb565);
    int m5_lcd_fill_circle(wasm_exec_env_t exec_env, int32_t x0, int32_t y0, int32_t r, uint32_t rgb565);

    // display text
    int m5_lcd_set_cursor(wasm_exec_env_t exec_env, int32_t x, int32_t y);
    int m5_lcd_set_text_color(wasm_exec_env_t exec_env, uint32_t fg, uint32_t bg);
    int m5_lcd_set_text_size(wasm_exec_env_t exec_env, uint32_t s);
    int m5_lcd_print(wasm_exec_env_t exec_env, uint32_t ptr, uint32_t len);
    int m5_lcd_print_float(wasm_exec_env_t exec_env, uint32_t ptr);

    // ====================
    // IMU
    // ====================
    // initialize
    bool m5_imu_is_enabled(wasm_exec_env_t exec_env);

    // get
    bool m5_imu_get_accel(wasm_exec_env_t exec_env, uint32_t ax_ptr, uint32_t ay_ptr, uint32_t az_ptr);
    bool m5_imu_get_gyro(wasm_exec_env_t exec_env, uint32_t gx_ptr, uint32_t gy_ptr, uint32_t gz_ptr);

#ifdef __cplusplus
}
#endif
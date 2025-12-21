#include "wasi_m5.h"

#include "M5Unified.h"

#include <M5ModuleDisplay.h>
#include "esp_log.h"

extern "C"
{

    int m5_setup(wasm_exec_env_t exec_env)
    {
        auto cfg = M5.config();
        M5.begin(cfg);
        return 0;
    }

    // ====================
    // Display
    // ====================

    // Display size
    int32_t m5_lcd_width(wasm_exec_env_t exec_env)
    {
        return M5.Display.width();
    }

    int32_t m5_lcd_height(wasm_exec_env_t exec_env)
    {
        return M5.Display.height();
    }

    // Display operations
    int m5_lcd_set_rotation(wasm_exec_env_t exec_env, int32_t r)
    {
        M5.Display.setRotation(r);
        return 0;
    }
    int m5_lcd_fill_screen(wasm_exec_env_t exec_env, uint32_t rgb565)
    {
        uint16_t color = (uint16_t)rgb565;
        M5.Display.fillScreen(color);
        return 0;
    }
    int m5_lcd_draw_pixel(wasm_exec_env_t exec_env, int32_t x, int32_t y, uint32_t rgb565)
    {
        uint16_t color = (uint16_t)rgb565;
        M5.Display.drawPixel(x, y, color);
        return 0;
    }
    int m5_lcd_draw_line(wasm_exec_env_t exec_env, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t rgb565)
    {
        uint16_t color = (uint16_t)rgb565;
        M5.Display.drawLine(x0, y0, x1, y1, color);
        return 0;
    }
    int m5_lcd_draw_rect(wasm_exec_env_t exec_env, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb565)
    {
        uint16_t color = (uint16_t)rgb565;
        M5.Display.drawRect(x, y, w, h, color);
        return 0;
    }
    int m5_lcd_fill_rect(wasm_exec_env_t exec_env, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb565)
    {
        uint16_t color = (uint16_t)rgb565;
        M5.Display.fillRect(x, y, w, h, color);
        return 0;
    }
    int m5_lcd_draw_circle(wasm_exec_env_t exec_env, int32_t x0, int32_t y0, int32_t r, uint32_t rgb565)
    {
        uint16_t color = (uint16_t)rgb565;
        M5.Display.drawCircle(x0, y0, r, color);
        return 0;
    }
    int m5_lcd_fill_circle(wasm_exec_env_t exec_env, int32_t x0, int32_t y0, int32_t r, uint32_t rgb565)
    {
        uint16_t color = (uint16_t)rgb565;
        M5.Display.fillCircle(x0, y0, r, color);
        return 0;
    }

    // display text
    int m5_lcd_set_cursor(wasm_exec_env_t exec_env, int32_t x, int32_t y)
    {
        M5.Display.setCursor((int16_t)x, (int16_t)y);
        return 0;
    }
    int m5_lcd_set_text_color(wasm_exec_env_t exec_env, uint32_t fg, uint32_t bg)
    {
        M5.Display.setTextColor((uint16_t)fg, (uint16_t)bg);
        return 0;
    }
    int m5_lcd_set_text_size(wasm_exec_env_t exec_env, uint32_t s)
    {
        M5.Display.setTextSize((int8_t)s);
        return 0;
    }
    int m5_lcd_print(wasm_exec_env_t exec_env, uint32_t ptr, uint32_t len)
    {
        wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);

        if (!wasm_runtime_validate_app_addr(inst, ptr, len))
        {
            printf("host_log: invalid memory range\n");
            return 1;
        }

        const char *guest_buf = (const char *)wasm_runtime_addr_app_to_native(inst, ptr);

        M5.Display.print(guest_buf);

        return 0;
    }

    int m5_lcd_print_float(wasm_exec_env_t exec_env, uint32_t ptr)
    {
        wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);

        if (!wasm_runtime_validate_app_addr(inst, ptr, sizeof(float)))
        {
            printf("host_log: invalid memory range\n");
            return 1;
        }

        const char *guest_buf = (const char *)wasm_runtime_addr_app_to_native(inst, ptr);
        float f = *(float *)guest_buf;
        M5.Display.print(f);

        return 0;
    }

    // ====================
    // IMU
    // ====================
    // initialize
    bool m5_imu_is_enabled(wasm_exec_env_t exec_env)
    {
        return M5.Imu.isEnabled();
    }

    // get
    // bool m5_imu_get_accel(wasm_exec_env_t exec_env, float *ax, float *ay, float *az)
    bool m5_imu_get_accel(wasm_exec_env_t exec_env, uint32_t ax_ptr, uint32_t ay_ptr, uint32_t az_ptr)
    {
        wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);

        if (!wasm_runtime_validate_app_addr(inst, ax_ptr, sizeof(float)) ||
            !wasm_runtime_validate_app_addr(inst, ay_ptr, sizeof(float)) ||
            !wasm_runtime_validate_app_addr(inst, az_ptr, sizeof(float)))
        {
            printf("host_log: invalid memory range\n");
            return 1;
        }
        float *ax = (float *)wasm_runtime_addr_app_to_native(inst, ax_ptr);
        float *ay = (float *)wasm_runtime_addr_app_to_native(inst, ay_ptr);
        float *az = (float *)wasm_runtime_addr_app_to_native(inst, az_ptr);

        return M5.Imu.getAccelData(ax, ay, az);
    }

    bool m5_imu_get_gyro(wasm_exec_env_t exec_env, uint32_t gx_ptr, uint32_t gy_ptr, uint32_t gz_ptr)
    {
        wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);

        if (!wasm_runtime_validate_app_addr(inst, gx_ptr, sizeof(float)) ||
            !wasm_runtime_validate_app_addr(inst, gy_ptr, sizeof(float)) ||
            !wasm_runtime_validate_app_addr(inst, gz_ptr, sizeof(float)))
        {
            printf("host_log: invalid memory range\n");
            return 1;
        }
        float *gx = (float *)wasm_runtime_addr_app_to_native(inst, gx_ptr);
        float *gy = (float *)wasm_runtime_addr_app_to_native(inst, gy_ptr);
        float *gz = (float *)wasm_runtime_addr_app_to_native(inst, gz_ptr);

        return M5.Imu.getGyroData(gx, gy, gz);
    }
}

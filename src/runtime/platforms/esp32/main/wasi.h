#pragma once

#include "wasm_export.h"

// I2C
int32_t waiot_i2c_param_config(wasm_exec_env_t exec_env, int32_t port,
                               int32_t sda_gpio, int32_t scl_gpio,
                               int32_t freq_hz);
int32_t waiot_i2c_driver_install(wasm_exec_env_t exec_env, int32_t port);
int32_t waiot_i2c_master_write(wasm_exec_env_t exec_env, int32_t port,
                               int32_t addr, int32_t write_buff_ptr_idx, int32_t write_size, int32_t ticks_to_wait);
int32_t waiot_i2c_master_read(wasm_exec_env_t exec_env, int32_t port,
                              int32_t addr, int32_t read_buff_ptr_idx, int32_t read_size, int32_t ticks_to_wait);

// GPIO
int32_t
gpio_set_pin_mode(wasm_exec_env_t exec_env, int32_t pin, int32_t dir);
int32_t
gpio_read(wasm_exec_env_t exec_env, int32_t pin);
int32_t
gpio_write(wasm_exec_env_t exec_env, int32_t pin, int32_t value);

// Delay
int32_t delay_ns(wasm_exec_env_t exec_env, int32_t ns);

// LEDC
int32_t pwm_init(wasm_exec_env_t exec_env, int32_t pin, int32_t channel, int32_t freq, int32_t resolution, int32_t speed_mode);
int32_t pwm_set_duty(wasm_exec_env_t exec_env, int32_t channel, int32_t duty, int32_t speed_mode);
int32_t pwm_update_duty(wasm_exec_env_t exec_env, int32_t channel, int32_t speed_mode);
int32_t pwm_set_frequency(wasm_exec_env_t exec_env, int32_t channel, int32_t freq, int32_t speed_mode);

// Logging (WASI: wasi_snapshot_review1.fd_write)
int32_t
fd_write(wasm_exec_env_t exec_env, int32_t fd, int32_t buf_iovec_addr,
         int32_t vec_len, int32_t size_addr);

// Log ring buffer accessors for HTTP serving
int32_t log_get_filled(void);
int32_t log_read_from_oldest(uint32_t offset, uint8_t *out, uint32_t len);

// HTTP client
void init_http_client();

int32_t http_init(wasm_exec_env_t exec_env, int32_t url_ptr,
                  int32_t url_len,
                  int32_t timeout_ms);
int32_t http_open(wasm_exec_env_t exec_env,
                      int32_t handle,
                      int32_t method,
                      int32_t content_len);

int32_t http_set_header(wasm_exec_env_t exec_env,
                    int32_t handle,
                    int32_t k_ptr, int32_t k_len,
                    int32_t v_ptr, int32_t v_len);

int32_t http_fetch_headers(wasm_exec_env_t exec_env, int32_t handle);

int32_t http_write(wasm_exec_env_t exec_env,
                       int32_t handle,
                       int32_t buf_ptr,
                       int32_t buf_len);

int32_t http_read(wasm_exec_env_t exec_env,
                  int32_t handle,
                  int32_t buf_ptr,
                  int32_t buf_len);

int32_t http_status(wasm_exec_env_t exec_env, int32_t handle);

int32_t http_close(wasm_exec_env_t exec_env, int32_t handle);

// Camera
int32_t if_camera_config_changed(wasm_exec_env_t exec_env, int pixel_format, int frame_size, int jpeg_quality);

int32_t camera_init(wasm_exec_env_t exec_env, int camera_device_type, int pixel_format, int frame_size, int jpeg_quality);

int32_t camera_get(wasm_exec_env_t exec_env, int32_t buf_ptr, int32_t buf_size);
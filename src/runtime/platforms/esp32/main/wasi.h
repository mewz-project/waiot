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

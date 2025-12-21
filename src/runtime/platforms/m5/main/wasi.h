#pragma once

#include "wasm_export.h"

// I2C
int32_t
i2c_open_bus(wasm_exec_env_t exec_env, int32_t port, int32_t sda_gpio,
             int32_t scl_gpio, int32_t freq_hz);
int32_t
i2c_write(wasm_exec_env_t exec_env, int32_t bus_handle, int32_t addr7,
          const uint8_t *data, uint32_t len, int32_t stop);
int32_t
i2c_read(wasm_exec_env_t exec_env, int32_t bus_handle, int32_t addr7,
         uint8_t *out, uint32_t len, int32_t stop);
int32_t
i2c_write_read(wasm_exec_env_t exec_env, int32_t bus_handle, int32_t addr7,
               const uint8_t *tx, uint32_t tx_len, uint8_t *rx,
               uint32_t rx_len);

// GPIO
int32_t
gpio_set_pin_mode(wasm_exec_env_t exec_env, int32_t pin, int32_t dir);
int32_t
gpio_read(wasm_exec_env_t exec_env, int32_t pin);
int32_t
gpio_write(wasm_exec_env_t exec_env, int32_t pin, int32_t value);

// Time
int64_t
mono_now_ns(wasm_exec_env_t exec_env);
int32_t
mono_sleep_ns(wasm_exec_env_t exec_env, int64_t ns);

// LEDC
int32_t waiot_ledc_init(wasm_exec_env_t exec_env, uint32_t pin, uint32_t channel, uint32_t freq, uint32_t resolution);
int32_t waiot_ledc_set_freq(wasm_exec_env_t exec_env, uint32_t channel, uint32_t freq);
int32_t waiot_ledc_set_duty(wasm_exec_env_t exec_env, uint32_t channel, uint32_t duty);
int32_t waiot_ledc_update_duty(wasm_exec_env_t exec_env, uint32_t channel);
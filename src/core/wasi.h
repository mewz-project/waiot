#pragma once

#include <stdint.h>
#include "wasm_export.h"

int32_t waiot_i2c_param_config(wasm_exec_env_t exec_env, int32_t port,
                               int32_t sda_gpio, int32_t scl_gpio,
                               int32_t freq_hz);
int32_t waiot_i2c_driver_install(wasm_exec_env_t exec_env, int32_t port);
int32_t waiot_i2c_master_write(wasm_exec_env_t exec_env, int32_t port,
                               int32_t addr, uint32_t write_buff_ptr_idx,
                               uint32_t write_size, int32_t ticks_to_wait);
int32_t waiot_i2c_master_read(wasm_exec_env_t exec_env, int32_t port,
                              int32_t addr, uint32_t read_buff_ptr_idx,
                              uint32_t read_size, int32_t ticks_to_wait);
int32_t waiot_i2c_master_write_read(wasm_exec_env_t exec_env, int32_t port,
                                    int32_t addr,
                                    uint32_t write_buff_ptr_idx,
                                    uint32_t write_size,
                                    uint32_t read_buff_ptr_idx,
                                    uint32_t read_size,
                                    int32_t ticks_to_wait);

int32_t gpio_set_pin_mode(wasm_exec_env_t exec_env, int32_t pin, int32_t dir);
int32_t gpio_read(wasm_exec_env_t exec_env, int32_t pin);
int32_t gpio_write(wasm_exec_env_t exec_env, int32_t pin, int32_t value);

int32_t delay_ns(wasm_exec_env_t exec_env, uint32_t ns);

int32_t pwm_init(wasm_exec_env_t exec_env, int32_t pin, int32_t channel,
                 int32_t freq, int32_t resolution, int32_t speed_mode);
int32_t pwm_set_duty(wasm_exec_env_t exec_env, int32_t channel, int32_t duty,
                     int32_t speed_mode);
int32_t pwm_update_duty(wasm_exec_env_t exec_env, int32_t channel,
                        int32_t speed_mode);
int32_t pwm_set_frequency(wasm_exec_env_t exec_env, int32_t channel,
                          int32_t freq, int32_t speed_mode);

int32_t waiot_spi_bus_init(wasm_exec_env_t exec_env, int32_t host,
                           int32_t mosi_gpio, int32_t miso_gpio,
                           int32_t sclk_gpio, int32_t max_transfer_sz);
int32_t waiot_spi_device_add(wasm_exec_env_t exec_env, int32_t host,
                             int32_t cs_gpio, int32_t mode, int32_t freq_hz);
int32_t waiot_spi_acquire(wasm_exec_env_t exec_env, int32_t device_handle);
int32_t waiot_spi_release(wasm_exec_env_t exec_env, int32_t device_handle);
int32_t waiot_spi_transfer(wasm_exec_env_t exec_env, int32_t device_handle,
                           int32_t tx_ptr, int32_t rx_ptr, uint32_t len);

int32_t fd_write(wasm_exec_env_t exec_env, int32_t fd, int32_t buf_iovec_addr,
                 int32_t vec_len, int32_t size_addr);

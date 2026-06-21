#include "wasi.h"

#include <stdint.h>

int32_t waiot_i2c_param_config(wasm_exec_env_t exec_env, int32_t port,
                               int32_t sda_gpio, int32_t scl_gpio,
                               int32_t freq_hz)
{
  (void)exec_env;
  (void)port;
  (void)sda_gpio;
  (void)scl_gpio;
  (void)freq_hz;
  return -1;
}

int32_t waiot_i2c_driver_install(wasm_exec_env_t exec_env, int32_t port)
{
  (void)exec_env;
  (void)port;
  return -1;
}

int32_t waiot_i2c_master_write(wasm_exec_env_t exec_env, int32_t port,
                               int32_t addr, int32_t write_buff_ptr_idx,
                               int32_t write_size, int32_t ticks_to_wait)
{
  (void)exec_env;
  (void)port;
  (void)addr;
  (void)write_buff_ptr_idx;
  (void)write_size;
  (void)ticks_to_wait;
  return -1;
}

int32_t waiot_i2c_master_read(wasm_exec_env_t exec_env, int32_t port,
                              int32_t addr, int32_t read_buff_ptr_idx,
                              int32_t read_size, int32_t ticks_to_wait)
{
  (void)exec_env;
  (void)port;
  (void)addr;
  (void)read_buff_ptr_idx;
  (void)read_size;
  (void)ticks_to_wait;
  return -1;
}

int32_t gpio_set_pin_mode(wasm_exec_env_t exec_env, int32_t pin, int32_t dir)
{
  (void)exec_env;
  (void)pin;
  (void)dir;
  return -1;
}

int32_t gpio_read(wasm_exec_env_t exec_env, int32_t pin)
{
  (void)exec_env;
  (void)pin;
  return -1;
}

int32_t gpio_write(wasm_exec_env_t exec_env, int32_t pin, int32_t value)
{
  (void)exec_env;
  (void)pin;
  (void)value;
  return -1;
}

int32_t pwm_init(wasm_exec_env_t exec_env, int32_t pin, int32_t channel,
                 int32_t freq, int32_t resolution, int32_t speed_mode)
{
  (void)exec_env;
  (void)pin;
  (void)channel;
  (void)freq;
  (void)resolution;
  (void)speed_mode;
  return -1;
}

int32_t pwm_set_duty(wasm_exec_env_t exec_env, int32_t channel, int32_t duty,
                     int32_t speed_mode)
{
  (void)exec_env;
  (void)channel;
  (void)duty;
  (void)speed_mode;
  return -1;
}

int32_t pwm_update_duty(wasm_exec_env_t exec_env, int32_t channel,
                        int32_t speed_mode)
{
  (void)exec_env;
  (void)channel;
  (void)speed_mode;
  return -1;
}

int32_t pwm_set_frequency(wasm_exec_env_t exec_env, int32_t channel,
                          int32_t freq, int32_t speed_mode)
{
  (void)exec_env;
  (void)channel;
  (void)freq;
  (void)speed_mode;
  return -1;
}

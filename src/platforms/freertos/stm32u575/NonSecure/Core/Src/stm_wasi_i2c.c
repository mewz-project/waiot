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

#include "wasi.h"

#include "cmsis_os2.h"
#include "wasm_export.h"

#include <stdint.h>

typedef struct
{
  int32_t iov_base;
  int32_t iov_len;
} wasi_iovec_t;

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

int32_t delay_ns(wasm_exec_env_t exec_env, int32_t ns)
{
  (void)exec_env;
  if (ns <= 0)
  {
    return 0;
  }

  uint32_t ms = ((uint32_t)ns + 999999U) / 1000000U;
  if (ms == 0U)
  {
    ms = 1U;
  }
  osDelay(ms);
  return 0;
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

int32_t fd_write(wasm_exec_env_t exec_env, int32_t fd, int32_t buf_iovec_addr,
                 int32_t vec_len, int32_t size_addr)
{
  if (fd != 1)
  {
    return -1;
  }

  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  if (!inst || vec_len < 0)
  {
    return -1;
  }

  uint32_t iovecs_size = (uint32_t)vec_len * (uint32_t)sizeof(wasi_iovec_t);
  if (!wasm_runtime_validate_app_addr(inst, buf_iovec_addr, iovecs_size))
  {
    return -1;
  }

  wasi_iovec_t *iovecs =
      (wasi_iovec_t *)wasm_runtime_addr_app_to_native(inst, buf_iovec_addr);
  uint32_t total_written = 0;
  for (int32_t i = 0; i < vec_len; i++)
  {
    if (iovecs[i].iov_len < 0)
    {
      return -1;
    }
    total_written += (uint32_t)iovecs[i].iov_len;
  }

  if (!wasm_runtime_validate_app_addr(inst, size_addr, sizeof(uint32_t)))
  {
    return -1;
  }
  uint32_t *nwritten_ptr =
      (uint32_t *)wasm_runtime_addr_app_to_native(inst, size_addr);
  *nwritten_ptr = total_written;
  return 0;
}

#include "wasi.h"

#include <stdint.h>

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

#include "wasi.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>

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

  TickType_t ticks = pdMS_TO_TICKS(ms);
  if (ticks == 0)
  {
    ticks = 1;
  }

  vTaskDelay(ticks);
  return 0;
}

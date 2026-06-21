#include "stm_waiot_entry.h"

#include "embedded_wasm.h"
#include "platform_wamr.h"
#include "wamr.h"

#include <stdio.h>

void stm_waiot_start(void)
{
  printf("waiot: start\r\n");

  waiot_platform_configure_wamr();
  init_wamr();

  int ret = wamr_set_wasm_binary(g_waiot_embedded_wasm, g_waiot_embedded_wasm_size);
  if (ret != 0)
  {
    printf("waiot: failed to set wasm binary: %d\r\n", ret);
    return;
  }

  launch_new_wasm_instance();
}

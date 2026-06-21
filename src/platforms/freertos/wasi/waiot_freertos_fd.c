#include "wasi.h"

#include "log.h"
#include "wasm_export.h"

#include <stdint.h>
#include <stdio.h>

typedef struct
{
  int32_t iov_base;
  int32_t iov_len;
} waiot_wasi_iovec_t;

int32_t fd_write(wasm_exec_env_t exec_env, int32_t fd, int32_t buf_iovec_addr,
                 int32_t vec_len, int32_t size_addr)
{
  if (fd != 1 && fd != 2)
  {
    return -1;
  }

  wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
  if (!inst || vec_len < 0)
  {
    return -1;
  }

  uint32_t iovecs_size =
      (uint32_t)vec_len * (uint32_t)sizeof(waiot_wasi_iovec_t);
  if (!wasm_runtime_validate_app_addr(inst, buf_iovec_addr, iovecs_size))
  {
    return -1;
  }

  waiot_wasi_iovec_t *iovecs =
      (waiot_wasi_iovec_t *)wasm_runtime_addr_app_to_native(inst,
                                                            buf_iovec_addr);
  if (!iovecs)
  {
    return -1;
  }

  uint32_t total_written = 0;
  for (int32_t i = 0; i < vec_len; i++)
  {
    int32_t base = iovecs[i].iov_base;
    int32_t len = iovecs[i].iov_len;
    if (len < 0)
    {
      return -1;
    }
    if (len == 0)
    {
      continue;
    }
    if (!wasm_runtime_validate_app_addr(inst, base, (uint32_t)len))
    {
      return -1;
    }

    const uint8_t *ptr =
        (const uint8_t *)wasm_runtime_addr_app_to_native(inst, base);
    if (!ptr)
    {
      return -1;
    }

    if (log_append(ptr, (uint32_t)len) != 0)
    {
      return -1;
    }

    for (int32_t j = 0; j < len; j++)
    {
      putchar((int)ptr[j]);
    }

    total_written += (uint32_t)len;
  }

  if (!wasm_runtime_validate_app_addr(inst, size_addr, sizeof(uint32_t)))
  {
    return -1;
  }

  uint32_t *nwritten_ptr =
      (uint32_t *)wasm_runtime_addr_app_to_native(inst, size_addr);
  if (!nwritten_ptr)
  {
    return -1;
  }

  *nwritten_ptr = total_written;
  return 0;
}

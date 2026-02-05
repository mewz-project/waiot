#pragma once

#include "wasi_nn.h"

#ifdef __cplusplus
extern "C"
{
#endif

    int esp_dl_load_simple(wasm_exec_env_t exec_env, uint32_t model_ptr_idx, uint32_t model_size);
    int esp_dl_init_execution_context_simple(wasm_exec_env_t exec_env);

#ifdef __cplusplus
}
#endif
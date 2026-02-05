#pragma once

#include "wasi_nn.h"

#ifdef __cplusplus
extern "C"
{
#endif

    int esp_dl_load_simple(wasm_exec_env_t exec_env, uint32_t model_ptr_idx, uint32_t model_size);
    int esp_dl_init_execution_context_simple(wasm_exec_env_t exec_env);
    int esp_dl_set_input_simple(wasm_exec_env_t exec_env, uint32_t input_ptr_idx, uint32_t input_size);
    int esp_dl_compute_simple(wasm_exec_env_t exec_env);
    int esp_dl_get_output_simple_idx(wasm_exec_env_t exec_env,
                                     uint32_t index,
                                     uint32_t output_ptr_idx,
                                     uint32_t output_buff_max_size);

#ifdef __cplusplus
}
#endif
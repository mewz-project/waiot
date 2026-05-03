#pragma once

#include "wasm_export.h"

#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct
    {
        uint32_t len;
        uint32_t *ptr;
    } wasi_nn_u32_list_t;

    typedef struct
    {
        uint32_t len;
        uint8_t *ptr;
    } wasi_nn_u8_list_t;

    typedef enum
    {
        success = 0,
        invalid_argument,
        invalid_encoding,
        missing_memory,
        busy,
        runtime_error,
        unsupported_operation,
        too_large,
        not_found,
    } wasi_nn_error;

    typedef uint32_t graph;
    typedef uint32_t graph_execution_context;

    typedef struct
    {
        uint32_t buf_offset;
        uint32_t size;
    } graph_builder_array;

    typedef enum
    {
        openvino = 0,
        onnx,
        tensorflow,
        pytorch,
        tensorflowlite,
        ggml,
        autodetect,
        unknown_backend,
    } graph_encoding;

    typedef enum execution_target
    {
        cpu = 0,
        gpu,
        tpu,
    } execution_target;

    typedef enum
    {
        fp16 = 0,
        fp32,
        up8,
        ip32,
    } tensor_type;

    typedef struct
    {
        wasi_nn_u32_list_t dimensions;
        uint8_t type;
        uint8_t _pad[3];
        wasi_nn_u8_list_t data;
    } tensor;

    int load(wasm_exec_env_t exec_env, graph_builder_array *builder, graph_encoding encoding,
             execution_target target, graph *g);
    int load_by_name(wasm_exec_env_t exec_env, const char *name, uint32_t namelen, graph *g);
    int init_execution_context(wasm_exec_env_t exec_env, graph g, graph_execution_context *exec_ctx);
    int set_input(wasm_exec_env_t exec_env, uint32_t exec_ctx, uint32_t index,
                  const tensor *input_tensor);
    int compute(wasm_exec_env_t exec_env, graph_execution_context exec_ctx);
    int get_output(wasm_exec_env_t exec_env, uint32_t exec_ctx, uint32_t index,
                   uint8_t *output_buff, uint32_t output_buff_max_size, uint32_t *output_size);
    int load_simple(wasm_exec_env_t exec_env, uint32_t model_ptr_idx, uint32_t model_size);
    int init_execution_context_simple(wasm_exec_env_t exec_env);
    int set_input_simple(wasm_exec_env_t exec_env, uint32_t input_ptr_idx, uint32_t input_size);
    int compute_simple(wasm_exec_env_t exec_env);
    int get_output_simple(wasm_exec_env_t exec_env,
                          uint32_t output_ptr_idx, uint32_t output_buff_max_size);

#ifdef __cplusplus
}
#endif

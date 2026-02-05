#include "wasi_nn_esp_dl.h"

#include "esp_log.h"

// ESP-DL
#include "dl_model_base.hpp"
#include "dl_tensor_base.hpp"
#include "fbs_model.hpp"

#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstring>

static const char *TAG = "wasi_nn_espdl";

static std::vector<uint8_t> g_model_storage;

static fbs::FbsModel *g_fbs_model = nullptr;
static dl::Model *g_model = nullptr;

static dl::TensorBase *g_input = nullptr;
static dl::TensorBase *g_output = nullptr;

static void cleanup_model()
{
    if (g_model)
    {
        delete g_model;
        g_model = nullptr;
    }
    if (g_fbs_model)
    {
        delete g_fbs_model;
        g_fbs_model = nullptr;
    }
    g_input = nullptr;
    g_output = nullptr;
    g_model_storage.clear();
}

static wasm_module_inst_t get_inst(wasm_exec_env_t exec_env)
{
    return wasm_runtime_get_module_inst(exec_env);
}

static bool app_range_ok(wasm_module_inst_t inst, uint32_t app_offset, uint32_t size)
{
    return inst && wasm_runtime_validate_app_addr(inst, app_offset, size);
}

static void *app_to_native(wasm_module_inst_t inst, uint32_t app_offset)
{
    return wasm_runtime_addr_app_to_native(inst, app_offset);
}

static int load_from_wasm_buffer(wasm_exec_env_t exec_env, uint32_t model_ptr_idx, uint32_t model_size)
{
    ESP_LOGI(TAG, "load_from_wasm_buffer: ptr=%u size=%u",
             (unsigned)model_ptr_idx, (unsigned)model_size);

    wasm_module_inst_t inst = get_inst(exec_env);
    if (!inst)
    {
        ESP_LOGE(TAG, "load_from_wasm_buffer: failed to get module instance");
        return invalid_argument;
    }
    if (!app_range_ok(inst, model_ptr_idx, model_size))
    {
        ESP_LOGE(TAG, "load_from_wasm_buffer: invalid memory range");
        return invalid_argument;
    }

    cleanup_model();

    // Copy model bytes from wasm memory
    uint8_t *src = (uint8_t *)app_to_native(inst, model_ptr_idx);
    g_model_storage.assign(src, src + model_size);

    // Create FbsModel from memory buffer
    g_fbs_model = new fbs::FbsModel(
        g_model_storage.data(),
        g_model_storage.size(),
        fbs::MODEL_LOCATION_IN_FLASH_RODATA,
        /*encrypt=*/false,
        /*rodata_move=*/false,
        /*auto_free=*/false,
        /*param_copy=*/true);

    if (!g_fbs_model)
    {
        ESP_LOGE(TAG, "load_from_wasm_buffer: FbsModel alloc failed");
        cleanup_model();
        return runtime_error;
    }

    g_model = new dl::Model(g_fbs_model, /*internal_size=*/0, dl::MEMORY_MANAGER_GREEDY);
    if (!g_model)
    {
        ESP_LOGE(TAG, "load_from_wasm_buffer: dl::Model alloc failed");
        cleanup_model();
        return runtime_error;
    }

    if (g_model->load(g_fbs_model) != ESP_OK)
    {
        ESP_LOGE(TAG, "load_from_wasm_buffer: dl::Model::load failed");
        cleanup_model();
        return invalid_argument;
    }

    // Allocate tensors / internal buffers
    g_model->build(/*max_internal_size=*/0, dl::MEMORY_MANAGER_GREEDY, /*preload=*/false);

    g_input = g_model->get_input();
    g_output = g_model->get_output();
    if (!g_input || !g_output)
    {
        ESP_LOGE(TAG, "load_from_wasm_buffer: get_input/get_output failed");
        cleanup_model();
        return runtime_error;
    }

    ESP_LOGI(TAG, "Model loaded: in_bytes=%d out_bytes=%d",
             g_input->get_bytes(), g_output->get_bytes());

    return success;
}

int esp_dl_load_simple(wasm_exec_env_t exec_env, uint32_t model_ptr_idx, uint32_t model_size)
{
    // Same loader core
    return load_from_wasm_buffer(exec_env, model_ptr_idx, model_size);
}
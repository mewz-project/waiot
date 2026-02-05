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

static dl::Model *g_model = nullptr;
static dl::TensorBase *g_input = nullptr;
static std::map<std::string, dl::TensorBase *> g_outputs_map;
static std::vector<std::string> g_output_names;

static constexpr const char *kModelPartitionLabel = "model";

static size_t round_up_4k(size_t n) { return (n + 4095) & ~((size_t)4095); }

static void cleanup_model()
{
    if (g_model)
    {
        delete g_model;
        g_model = nullptr;
    }
    g_input = nullptr;
    g_outputs_map.clear();
    g_output_names.clear();
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

    // ===============================
    // Store model to partition
    // ===============================
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        kModelPartitionLabel);
    if (!part)
    {
        ESP_LOGE(TAG, "model partition '%s' not found", kModelPartitionLabel);
        return not_found;
    }

    if (model_size > part->size)
    {
        ESP_LOGE(TAG, "model too large: %u > partition %u", (unsigned)model_size, (unsigned)part->size);
        return too_large;
    }
    // erase partition
    size_t erase_sz = round_up_4k(model_size);
    esp_err_t err = esp_partition_erase_range(part, 0, erase_sz);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "erase failed: %s", esp_err_to_name(err));
        return runtime_error;
    }
    // write model data
    size_t write_sz = (model_size + 3) & ~((size_t)3);
    uint8_t tail_pad[4] = {0, 0, 0, 0};
    if (write_sz == model_size)
    {
        err = esp_partition_write(part, 0, src, model_size);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "write failed: %s", esp_err_to_name(err));
            return runtime_error;
        }
    }
    else
    {
        // body
        err = esp_partition_write(part, 0, src, model_size);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "write body failed: %s", esp_err_to_name(err));
            return runtime_error;
        }
        // padding
        size_t pad_len = write_sz - model_size;
        err = esp_partition_write(part, model_size, tail_pad, pad_len);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "write pad failed: %s", esp_err_to_name(err));
            return runtime_error;
        }
    }

    // ===============================
    // load model
    // ===============================
    bool param_copy = true;
    g_model = new dl::Model(kModelPartitionLabel, fbs::MODEL_LOCATION_IN_FLASH_PARTITION,
                            /*max_internal_size=*/0, dl::MEMORY_MANAGER_GREEDY,
                            /*key=*/nullptr, /*param_copy=*/param_copy);

    if (!g_model)
    {
        ESP_LOGE(TAG, "dl::Model alloc failed");
        return runtime_error;
    }
    // if (g_model->load(kModelPartitionLabel, fbs::MODEL_LOCATION_IN_FLASH_PARTITION, nullptr, param_copy) != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "Model::load failed");
    //     cleanup_model();
    //     return invalid_argument;
    // }
    
    // Allocate tensors / internal buffers
    g_model->build(/*max_internal_size=*/0, dl::MEMORY_MANAGER_GREEDY, /*preload=*/false);

    g_input = g_model->get_input();
    g_outputs_map = g_model->get_outputs();
    g_output_names.reserve(g_outputs_map.size());
    for (const auto &kv : g_outputs_map)
    {
        g_output_names.push_back(kv.first);
    }

    if (!g_input || g_outputs_map.empty())
    {
        ESP_LOGE(TAG, "load_from_wasm_buffer: get_input/get_output failed");
        cleanup_model();
        return runtime_error;
    }

    ESP_LOGI(TAG, "Model loaded: in_bytes=%d",
             g_input->get_bytes());
    for (const auto &name : g_output_names)
    {
        dl::TensorBase *output = g_outputs_map[name];
        ESP_LOGI(TAG, "  output '%s': out_bytes=%d",
                 name.c_str(), output->get_bytes());
    }
    return success;
}

int esp_dl_load_simple(wasm_exec_env_t exec_env, uint32_t model_ptr_idx, uint32_t model_size)
{
    // Same loader core
    return load_from_wasm_buffer(exec_env, model_ptr_idx, model_size);
}
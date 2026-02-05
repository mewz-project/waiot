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

// Load model from memory
int esp_dl_load_simple(wasm_exec_env_t exec_env, uint32_t model_ptr_idx, uint32_t model_size)
{
    // Same loader core
    return load_from_wasm_buffer(exec_env, model_ptr_idx, model_size);
}

// Initialize execution context
// Only check if model is loaded
int esp_dl_init_execution_context_simple(wasm_exec_env_t exec_env)
{
    (void)exec_env;

    if (!g_model || !g_input || g_outputs_map.empty())
    {
        ESP_LOGE(TAG, "init_execution_context_simple: model not loaded");
        return runtime_error;
    }

    ESP_LOGI(TAG, "init_execution_context_simple: ok (outputs=%u)", (unsigned)g_output_names.size());
    return success;
}

int esp_dl_set_input_simple(wasm_exec_env_t exec_env, uint32_t input_ptr_idx, uint32_t input_size)
{
    ESP_LOGI(TAG, "set_input_simple: ptr=%u size=%u",
             (unsigned)input_ptr_idx, (unsigned)input_size);
    // Check model and input tensor
    if (!g_input)
    {
        ESP_LOGE(TAG, "set_input_simple: model input not ready");
        return runtime_error;
    }
    wasm_module_inst_t inst = get_inst(exec_env);
    if (!inst)
    {
        ESP_LOGE(TAG, "set_input_simple: failed to get module instance");
        return invalid_argument;
    }
    if (!app_range_ok(inst, input_ptr_idx, input_size))
    {
        ESP_LOGE(TAG, "set_input_simple: invalid memory range ptr=%u size=%u",
                 (unsigned)input_ptr_idx, (unsigned)input_size);
        return invalid_argument;
    }

    const int expected = g_input->get_bytes();
    if ((int)input_size != expected)
    {
        ESP_LOGW(TAG, "set_input_simple: size mismatch got=%u expected=%d (zero-copy uses expected size!)",
                 (unsigned)input_size, expected);
    }

    void *native_ptr = app_to_native(inst, input_ptr_idx);

    if (((uintptr_t)native_ptr & 0xF) != 0)
    {
        ESP_LOGW(TAG, "set_input_simple: input ptr not 16B-aligned: %p (may hurt perf or break on some kernels)", native_ptr);
    }

    g_input->set_element_ptr(native_ptr);

    // Copy version
    // void *src = app_to_native(inst, input_ptr_idx);
    // void *dst = g_input->get_element_ptr();
    // if (!dst)
    // {
    //     ESP_LOGE(TAG, "set_input_simple: input element ptr is null");
    //     return runtime_error;
    // }

    // int expected = g_input->get_bytes();
    // size_t copy_sz = std::min<size_t>((size_t)input_size, (size_t)expected);

    // if ((int)input_size != expected)
    // {
    //     ESP_LOGW(TAG, "set_input_simple: size mismatch got=%u expected=%d (copy %u)",
    //              (unsigned)input_size, expected, (unsigned)copy_sz);
    // }

    // memcpy(dst, src, copy_sz);

    ESP_LOGI(TAG, "set_input_simple: done");
    return success;
}

int esp_dl_compute_simple(wasm_exec_env_t exec_env)
{
    (void)exec_env;
    ESP_LOGI(TAG, "compute_simple...");

    if (!g_model)
    {
        ESP_LOGE(TAG, "compute_simple: model not loaded");
        return runtime_error;
    }

    g_model->run(dl::RUNTIME_MODE_SINGLE_CORE);

    ESP_LOGI(TAG, "compute_simple: done");
    return success;
}

static dl::TensorBase *get_output_tensor_by_index(uint32_t index)
{
    if (index >= g_output_names.size())
    {
        return nullptr;
    }
    const std::string &name = g_output_names[index];
    auto it = g_outputs_map.find(name);
    if (it == g_outputs_map.end())
    {
        return nullptr;
    }
    return it->second;
}

int esp_dl_get_output_simple_idx(wasm_exec_env_t exec_env,
                                 uint32_t index,
                                 uint32_t output_ptr_idx,
                                 uint32_t output_buff_max_size)
{
    dl::TensorBase *out = get_output_tensor_by_index(index);
    if (!out)
    {
        ESP_LOGE(TAG, "get_output_simple_idx: invalid index=%u (num=%u)",
                 (unsigned)index, (unsigned)g_output_names.size());
        return invalid_argument;
    }

    int out_bytes = out->get_bytes();
    if (output_buff_max_size < (uint32_t)out_bytes)
    {
        ESP_LOGE(TAG, "get_output_simple_idx: buffer too small need=%d max=%u",
                 out_bytes, (unsigned)output_buff_max_size);
        return too_large;
    }

    wasm_module_inst_t inst = get_inst(exec_env);
    if (!inst)
    {
        ESP_LOGE(TAG, "get_output_simple_idx: failed to get module instance");
        return invalid_argument;
    }
    if (!app_range_ok(inst, output_ptr_idx, (uint32_t)out_bytes))
    {
        ESP_LOGE(TAG, "get_output_simple_idx: invalid memory range ptr=%u size=%d",
                 (unsigned)output_ptr_idx, out_bytes);
        return invalid_argument;
    }

    void *dst = app_to_native(inst, output_ptr_idx);
    void *src = out->get_element_ptr();
    if (!src)
    {
        ESP_LOGE(TAG, "get_output_simple_idx: output element ptr is null");
        return runtime_error;
    }

    memcpy(dst, src, (size_t)out_bytes);

    ESP_LOGD(TAG, "get_output_simple_idx: index=%u name=%s bytes=%d",
             (unsigned)index, g_output_names[index].c_str(), out_bytes);
    ESP_LOGI(TAG, "%d %d %d %d %d %d %d %d",
             ((int32_t *)dst)[0], ((int32_t *)dst)[1], ((int32_t *)dst)[2], ((int32_t *)dst)[3],
             ((int32_t *)dst)[4], ((int32_t *)dst)[5], ((int32_t *)dst)[6], ((int32_t *)dst)[7]);

    ESP_LOGI(TAG, "get_output_simple_idx: done");
    return success;
}
#include "platform_wamr.h"

#include "config.h"
#include "wamr.h"
#include "wasi_nn.h"

#include "bh_platform.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "wasm_export.h"

void waiot_platform_configure_wamr(void)
{
    wamr_set_allocator((void *)os_malloc, (void *)os_realloc, (void *)os_free);
}

void waiot_platform_print_memory_info(void)
{
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    ESP_LOGI("MEM", "Free heap: %u bytes, Min free since boot: %u bytes",
             (unsigned)free_heap, (unsigned)min_free);
}

void waiot_platform_register_extra_natives(void)
{
#if CONFIG_USE_TFLM
    static NativeSymbol wasi_nn_syms[] = {
        {"load", load, "(*ii*)i", NULL},
        {"load_by_name", load_by_name, "(*i*)i", NULL},
        {"init_execution_context", init_execution_context, "(i*)i", NULL},
        {"set_input", set_input, "(ii*)i", NULL},
        {"compute", compute, "(i)i", NULL},
        {"get_output", get_output, "(ii*i*)i", NULL},
        {"load_simple", load_simple, "(ii)i", NULL},
        {"init_execution_context_simple", init_execution_context_simple, "()i", NULL},
        {"set_input_simple", set_input_simple, "(ii)i", NULL},
        {"compute_simple", compute_simple, "()i", NULL},
        {"get_output_simple", get_output_simple, "(ii)i", NULL},
    };

    if (!wasm_runtime_register_natives(
            "wasi:wasi_nn", wasi_nn_syms,
            (uint32_t)(sizeof(wasi_nn_syms) / sizeof(NativeSymbol))))
    {
        ESP_LOGE(LOG_TAG, "Failed to register WASI-NN native symbols.");
    }
#endif
}

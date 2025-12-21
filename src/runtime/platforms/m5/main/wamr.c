#include "wamr.h"

#include "wasi.h"
#include "wasi_m5.h"
#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wasm_export.h"
#include "bh_platform.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_log.h"

static uint8_t *g_uploaded_data = NULL;
static size_t g_uploaded_size = 0;

wasm_module_inst_t wasm_module_inst = NULL;
pthread_t thread_wamr;

/* ===== Wasm ===== */
static NativeSymbol i2c_syms[] = {
    {"open_bus", i2c_open_bus, "(iiii)i", NULL},
    {"write", i2c_write, "(ii*~i)i", NULL},
    {"read", i2c_read, "(ii*~i)i", NULL},
    {"write-read", i2c_write_read, "(ii*~*~)i", NULL},
};

static NativeSymbol gpio_syms[] = {
    {"gpio_set_pin_mode", gpio_set_pin_mode, "(ii)i", NULL},
    {"gpio_read", gpio_read, "(i)i", NULL},
    {"gpio_write", gpio_write, "(ii)i", NULL},
};

static NativeSymbol clock_syms[] = {
    {"now", mono_now_ns, "()I", NULL},
    {"sleep", mono_sleep_ns, "(I)i", NULL},
};

static NativeSymbol ledc_syms[] = {
    {"ledc_init", waiot_ledc_init, "(iiii)i", NULL},
    {"ledc_set_freq", waiot_ledc_set_freq, "(ii)i", NULL},
    {"ledc_set_duty", waiot_ledc_set_duty, "(ii)i", NULL},
    {"ledc_update_duty", waiot_ledc_update_duty, "(i)i", NULL},
};

static NativeSymbol m5_syms[] = {
    {"m5_setup", m5_setup, "()i", NULL},
    // Display
    {"m5_lcd_width", m5_lcd_width, "()i", NULL},
    {"m5_lcd_height", m5_lcd_height, "()i", NULL},
    {"m5_lcd_set_rotation", m5_lcd_set_rotation, "(i)i", NULL},
    {"m5_lcd_fill_screen", m5_lcd_fill_screen, "(i)i", NULL},
    {"m5_lcd_draw_pixel", m5_lcd_draw_pixel, "(iii)i", NULL},
    {"m5_lcd_draw_line", m5_lcd_draw_line, "(iiiii)i", NULL},
    {"m5_lcd_draw_rect", m5_lcd_draw_rect, "(iiiiii)i", NULL},
    {"m5_lcd_fill_rect", m5_lcd_fill_rect, "(iiiiii)i", NULL},
    {"m5_lcd_draw_circle", m5_lcd_draw_circle, "(iiii)i", NULL},
    {"m5_lcd_fill_circle", m5_lcd_fill_circle, "(iiii)i", NULL},
    {"m5_lcd_set_cursor", m5_lcd_set_cursor, "(ii)i", NULL},
    {"m5_lcd_set_text_color", m5_lcd_set_text_color, "(ii)i", NULL},
    {"m5_lcd_set_text_size", m5_lcd_set_text_size, "(i)i", NULL},
    {"m5_lcd_print", m5_lcd_print, "(ii)i", NULL},
    {"m5_lcd_print_float", m5_lcd_print_float, "(i)i", NULL},
    // IMU
    {"m5_imu_is_enabled", m5_imu_is_enabled, "()i", NULL},
    {"m5_imu_get_accel", m5_imu_get_accel, "(iii)i", NULL},
    {"m5_imu_get_gyro", m5_imu_get_gyro, "(iii)i", NULL},
};

void init_wamr()
{
    /* setup variables for instantiating and running the wasm module */
    RuntimeInitArgs init_args;

    /* configure memory allocation */
    memset(&init_args, 0, sizeof(RuntimeInitArgs));
#if WASM_ENABLE_GLOBAL_HEAP_POOL == 0
    init_args.mem_alloc_type = Alloc_With_Allocator;
    init_args.mem_alloc_option.allocator.malloc_func = (void *)os_malloc;
    init_args.mem_alloc_option.allocator.realloc_func = (void *)os_realloc;
    init_args.mem_alloc_option.allocator.free_func = (void *)os_free;
#else
#error The usage of a global heap pool is not implemented yet for esp-idf.
#endif

    ESP_LOGI(LOG_TAG, "Initialize WASM runtime");
    // print_mem_info();

    /* initialize runtime environment */
    if (!wasm_runtime_full_init(&init_args))
    {
        ESP_LOGE(LOG_TAG, "Init runtime failed.");
        return;
    }

    if (!wasm_runtime_register_natives(
            "wasi:i2c/i2c", i2c_syms,
            (uint32_t)(sizeof(i2c_syms) / sizeof(NativeSymbol))))
    {
        ESP_LOGE(LOG_TAG, "Failed to register I2C native symbols.");
        return;
    }
    if (!wasm_runtime_register_natives(
            "wasi:digital", gpio_syms,
            (uint32_t)(sizeof(gpio_syms) / sizeof(NativeSymbol))))
    {
        ESP_LOGE(LOG_TAG, "Failed to register GPIO native symbols.");
        return;
    }
    if (!wasm_runtime_register_natives(
            "wasi:clock/mono", clock_syms,
            (uint32_t)(sizeof(clock_syms) / sizeof(NativeSymbol))))
    {
        ESP_LOGE(LOG_TAG, "Failed to register Clock native symbols.");
        return;
    }
    if (!wasm_runtime_register_natives(
            "wasi:ledc/ledc", ledc_syms,
            (uint32_t)(sizeof(ledc_syms) / sizeof(NativeSymbol))))
    {
        ESP_LOGE(LOG_TAG, "Failed to register LEDC native symbols.");
        return;
    }

    if (!wasm_runtime_register_natives(
            "wasi:m5", m5_syms,
            (uint32_t)(sizeof(m5_syms) / sizeof(NativeSymbol))))
    {
        ESP_LOGE(LOG_TAG, "Failed to register M5 native symbols.");
        return;
    }
}

void *
run_wamr()
{
    uint8_t *wasm_file_buf = NULL;
    unsigned wasm_file_buf_size = 0;
    wasm_module_t wasm_module = NULL;
    char error_buf[128];

    ESP_LOGI(LOG_TAG, "Run wamr with interpreter");

    // TODO: Why Wasm binary is overwritten?
    wasm_file_buf_size = g_uploaded_size;
    wasm_file_buf = malloc(g_uploaded_size);
    memcpy(wasm_file_buf, g_uploaded_data, g_uploaded_size);

    /* load WASM module */
    if (!(wasm_module = wasm_runtime_load(wasm_file_buf, wasm_file_buf_size,
                                          error_buf, sizeof(error_buf))))
    {
        ESP_LOGE(LOG_TAG, "Error in wasm_runtime_load: %s", error_buf);
        goto fail;
    }

    ESP_LOGI(LOG_TAG, "Instantiate WASM runtime");
    if (!(wasm_module_inst =
              wasm_runtime_instantiate(wasm_module, 32 * 1024, // stack size
                                       32 * 1024,              // heap size
                                       error_buf, sizeof(error_buf))))
    {
        ESP_LOGE(LOG_TAG, "Error while instantiating: %s", error_buf);
        goto fail;
    }

    // creating env is required for wasm_runtime_terminate
    wasm_exec_env_t _ = wasm_runtime_get_exec_env_singleton(wasm_module_inst);

    bool ok = wasm_runtime_init_thread_env();
    assert(ok);

    // run main()
    // Once wasm_runtime_terminate is call, this function returns at the next safe point.
    wasm_application_execute_main(wasm_module_inst, 0, NULL);

    // Destroy thread environment
    wasm_runtime_destroy_thread_env();

    // Destroy instance
    wasm_runtime_deinstantiate(wasm_module_inst);
    wasm_module_inst = NULL;

fail:
    /* unload the module */
    ESP_LOGI(LOG_TAG, "Unload WASM module");
    wasm_runtime_unload(wasm_module);
    return NULL;
}

void stop_current_wasm_instance()
{
    if (wasm_module_inst)
    {
        ESP_LOGW(LOG_TAG, "Terminating previous Wasm runtime…");
        wasm_runtime_terminate(wasm_module_inst);

        // Wait for the previous thread to finish
        pthread_join(thread_wamr, NULL);

        ESP_LOGI(LOG_TAG, "Previous Wasm runtime terminated.");
    }
}

void launch_new_wasm_instance()
{
    pthread_attr_t tattr;
    pthread_attr_init(&tattr);
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&tattr, IWASM_MAIN_STACK_SIZE);

    // Start the thread
    // Don't wait for finish because it may run forever.
    int res = pthread_create(&thread_wamr, &tattr, run_wamr, (void *)NULL);
    assert(res == 0);
    pthread_attr_destroy(&tattr);
}

esp_err_t receive_wasm_binary(httpd_req_t *req)
{
    // Check request
    if (req->content_len <= 0)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
    }
    if (req->content_len > REGISTER_MAX_UPLOAD_SIZE)
    {
        ESP_LOGW(LOG_TAG, "Upload too large: %d > %d", (int)req->content_len,
                 (int)REGISTER_MAX_UPLOAD_SIZE);
        return httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE,
                                   "File too large");
    }

    // Cleanup previous wasm
    if (g_uploaded_data)
    {
        free(g_uploaded_data);
        g_uploaded_data = NULL;
        g_uploaded_size = 0;
    }

    // Allocate buffer
    uint8_t *buf = (uint8_t *)malloc(req->content_len + 1);
    if (!buf)
    {
        ESP_LOGE(LOG_TAG, "OOM while allocating %d bytes", (int)req->content_len);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    }

    // Read data
    size_t remaining = req->content_len;
    size_t offset = 0;
    while (remaining > 0)
    {
        size_t to_read = remaining;
        if (to_read > 4096)
            to_read = 4096;

        int r = httpd_req_recv(req, (char *)buf + offset, to_read);
        if (r <= 0)
        {
            if (r == HTTPD_SOCK_ERR_TIMEOUT)
            {
                // keep reading on timeout
                continue;
            }
            ESP_LOGE(LOG_TAG, "httpd_req_recv failed: %d", r);
            free(buf);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "recv failed");
        }
        offset += r;
        remaining -= r;
    }
    buf[offset] = 0;

    g_uploaded_data = buf;
    g_uploaded_size = offset;
    return ESP_OK;
}

bool is_wasm_instance_running()
{
    return wasm_module_inst != NULL;
}
#include "wamr.h"

#include "wasi.h"
#include "wasi_nn.h"
#include "pinmap.h"
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

#include <stdatomic.h>
#include <pthread.h>

// Wasm binary upload buffer
static uint8_t *g_uploaded_data = NULL;
static size_t g_uploaded_size = 0;

// WAMR module and instance
wasm_module_inst_t wasm_module_inst = NULL;
pthread_t thread_wamr;

// Flags for thread management
static atomic_bool g_wamr_thread_running = ATOMIC_VAR_INIT(false);
static atomic_bool g_wamr_thread_joined = ATOMIC_VAR_INIT(true);
static pthread_mutex_t g_wamr_thread_mu = PTHREAD_MUTEX_INITIALIZER;

/* ===== Wasm ===== */
static NativeSymbol i2c_syms[] = {
    {"i2c_param_config", waiot_i2c_param_config, "(iiii)i", NULL},
    {"i2c_driver_install", waiot_i2c_driver_install, "(i)i", NULL},
    {"i2c_master_write", waiot_i2c_master_write, "(iiiii)i", NULL},
    {"i2c_master_read", waiot_i2c_master_read, "(iiiii)i", NULL},
};

static NativeSymbol gpio_syms[] = {
    {"gpio_set_pin_mode", gpio_set_pin_mode, "(ii)i", NULL},
    {"gpio_read", gpio_read, "(i)i", NULL},
    {"gpio_write", gpio_write, "(ii)i", NULL},
};

static NativeSymbol delay_syms[] = {
    {"delay_ns", delay_ns, "(i)i", NULL},
};

static NativeSymbol pwm_syms[] = {
    {"pwm_init", pwm_init, "(iiiii)i", NULL},
    {"pwm_set_duty", pwm_set_duty, "(iii)i", NULL},
    {"pwm_update_duty", pwm_update_duty, "(ii)i", NULL},
    {"pwm_set_frequency", pwm_set_frequency, "(iii)i", NULL},
};

static NativeSymbol wasi_snapshot_preview1_syms[] = {
    {"fd_write", fd_write, "(iiii)i", NULL},
};

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
#endif // CONFIG_USE_TFLM

void print_free_heap(void)
{
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    ESP_LOGI("MEM", "Free heap: %u bytes, Min free since boot: %u bytes",
             (unsigned)free_heap, (unsigned)min_free);
}

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
            "wasi:i2c", i2c_syms,
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
            "wasi:delay", delay_syms,
            (uint32_t)(sizeof(delay_syms) / sizeof(NativeSymbol))))
    {
        ESP_LOGE(LOG_TAG, "Failed to register Delay native symbols.");
        return;
    }

    if (!wasm_runtime_register_natives(
            "wasi:pwm", pwm_syms,
            (uint32_t)(sizeof(pwm_syms) / sizeof(NativeSymbol))))
    {
        ESP_LOGE(LOG_TAG, "Failed to register PWM native symbols.");
        return;
    }

    if (!wasm_runtime_register_natives(
            "wasi:wasi_snapshot_preview1", wasi_snapshot_preview1_syms,
            (uint32_t)(sizeof(wasi_snapshot_preview1_syms) / sizeof(NativeSymbol))))
    {
        ESP_LOGE(LOG_TAG, "Failed to register WASI fd_write native symbol.");
        return;
    }

#if CONFIG_USE_TFLM
    if (!wasm_runtime_register_natives(
            "wasi:wasi_nn", wasi_nn_syms,
            (uint32_t)(sizeof(wasi_nn_syms) / sizeof(NativeSymbol))))
    {
        ESP_LOGE(LOG_TAG, "Failed to register WASI-NN native symbols.");
        return;
    }
#endif // CONFIG_USE_TFLM
}

void *
run_wamr()
{
    atomic_store(&g_wamr_thread_running, true);

    unsigned wasm_file_buf_size = 0;
    uint8_t *wasm_file_buf = NULL;

    wasm_module_t wasm_module = NULL;
    char error_buf[128];
    bool if_created_env = false;

    ESP_LOGI(LOG_TAG, "Run wamr with interpreter");
    print_free_heap();

    // TODO: Why Wasm binary is overwritten?
    pthread_mutex_lock(&g_wamr_thread_mu);
    if (!g_uploaded_data || g_uploaded_size == 0)
    {
        ESP_LOGE(LOG_TAG, "No Wasm binary uploaded");
        pthread_mutex_unlock(&g_wamr_thread_mu);
        goto done;
    }
    wasm_file_buf_size = g_uploaded_size;
    wasm_file_buf = malloc(g_uploaded_size);
    if (!wasm_file_buf)
    {
        ESP_LOGE(LOG_TAG, "Failed allocating %d bytes for Wasm binary",
                 (int)g_uploaded_size);
        pthread_mutex_unlock(&g_wamr_thread_mu);
        goto done;
    }
    memcpy(wasm_file_buf, g_uploaded_data, g_uploaded_size);
    pthread_mutex_unlock(&g_wamr_thread_mu);

    // init thread env
    if (!wasm_runtime_init_thread_env())
    {
        ESP_LOGE(LOG_TAG, "Failed to init thread env");
        goto done;
    }
    if_created_env = true;

    // load WASM module
    if (!(wasm_module = wasm_runtime_load(wasm_file_buf, wasm_file_buf_size,
                                          error_buf, sizeof(error_buf))))
    {
        ESP_LOGE(LOG_TAG, "Error in wasm_runtime_load: %s", error_buf);
        goto done;
    }

    // instantiate WASM module
    ESP_LOGI(LOG_TAG, "Instantiate WASM runtime");
    pthread_mutex_lock(&g_wamr_thread_mu);
    if (!(wasm_module_inst =
              wasm_runtime_instantiate(wasm_module, 32 * 1024, // stack size
                                       32 * 1024,              // heap size
                                       error_buf, sizeof(error_buf))))
    {
        ESP_LOGE(LOG_TAG, "Error while instantiating: %s", error_buf);
        pthread_mutex_unlock(&g_wamr_thread_mu);
        goto done;
    }
    pthread_mutex_unlock(&g_wamr_thread_mu);

    // creating env is required for wasm_runtime_terminate
    wasm_exec_env_t exec_env = wasm_runtime_get_exec_env_singleton(wasm_module_inst);
    if (!exec_env)
    {
        ESP_LOGE(LOG_TAG, "Failed to get exec env");
        goto done;
    }

    
    // run main()
    // Once wasm_runtime_terminate is call, this function returns at the next safe point.
    if (!wasm_application_execute_main(wasm_module_inst, 0, NULL))
    {
        ESP_LOGE(LOG_TAG, "Failed to execute main()");
        goto done;
    }

done:
    if (wasm_module_inst)
    {
        wasm_runtime_deinstantiate(wasm_module_inst);
        wasm_module_inst = NULL;
    }
    if (wasm_module) {
        wasm_runtime_unload(wasm_module);
        wasm_module = NULL;
    }
    if (wasm_file_buf) {
        free(wasm_file_buf);
        wasm_file_buf = NULL;
    }
    if (if_created_env)
    {
        wasm_runtime_destroy_thread_env();
    }
    print_free_heap();

    atomic_store(&g_wamr_thread_running, false);
    return NULL;
}

void stop_current_wasm_instance()
{
    pthread_t t_for_join = 0;
    bool need_join = false;

    pthread_mutex_lock(&g_wamr_thread_mu);

    // Terminate the runtime if still running
    if (wasm_module_inst)
    {
        ESP_LOGW(LOG_TAG, "Terminating previous Wasm runtime…");
        wasm_runtime_terminate(wasm_module_inst);
    }

    // Check if we need to join and keep the thread handle
    if (!atomic_load(&g_wamr_thread_joined))
    {
        t_for_join = thread_wamr;
        need_join = true;
        atomic_store(&g_wamr_thread_joined, true);
    }
    pthread_mutex_unlock(&g_wamr_thread_mu);

    // Join outside of mutex
    if (need_join) {
        pthread_join(t_for_join, NULL);
        ESP_LOGI(LOG_TAG, "Previous Wasm runtime terminated/joined.");
    }

    print_free_heap();
}

void launch_new_wasm_instance()
{
    stop_current_wasm_instance();

    pthread_attr_t tattr;
    pthread_attr_init(&tattr);
    pthread_attr_setstacksize(&tattr, IWASM_MAIN_STACK_SIZE);

    // Start the thread
    // Don't wait for finish because it may run forever.
    pthread_mutex_lock(&g_wamr_thread_mu);
    atomic_store(&g_wamr_thread_joined, false);
    int res = pthread_create(&thread_wamr, &tattr, run_wamr, (void *)NULL);
    if (res != 0)
    {
        ESP_LOGE(LOG_TAG, "Failed to create WAMR thread: %d", res);
        atomic_store(&g_wamr_thread_joined, true);
    }
    
    pthread_mutex_unlock(&g_wamr_thread_mu);
    
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

    // Lock mutex
    pthread_mutex_lock(&g_wamr_thread_mu);
    
    int ret;
    
    // Assert if current Wasm instance is not running
    if (is_wasm_instance_running()) {
        ESP_LOGW(LOG_TAG, "Cannot receive Wasm binary: instance is running");
        ret = httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                      "Wasm instance is running");
        free(buf);
        goto out;
    }

    // Cleanup previous wasm
    if (g_uploaded_data)
    {
        free(g_uploaded_data);
        g_uploaded_data = NULL;
        g_uploaded_size = 0;
    }

    // Replace with new wasm
    g_uploaded_data = buf;
    g_uploaded_size = offset;
    ret = ESP_OK;
out:
    pthread_mutex_unlock(&g_wamr_thread_mu);
    return ret;
}

bool is_wasm_instance_running()
{
    return atomic_load(&g_wamr_thread_running);
}

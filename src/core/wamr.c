#include "wamr.h"

#include "platform_thread.h"
#include "wasi.h"
#include "wasm_export.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef IWASM_MAIN_STACK_SIZE
#define IWASM_MAIN_STACK_SIZE 8192
#endif

#if WASM_ENABLE_GLOBAL_HEAP_POOL != 0
#ifndef WAIOT_WAMR_GLOBAL_HEAP_SIZE
#ifdef WASM_GLOBAL_HEAP_SIZE
#define WAIOT_WAMR_GLOBAL_HEAP_SIZE WASM_GLOBAL_HEAP_SIZE
#else
#define WAIOT_WAMR_GLOBAL_HEAP_SIZE (64 * 1024)
#endif
#endif
static uint8_t g_wamr_global_heap[WAIOT_WAMR_GLOBAL_HEAP_SIZE]
    __attribute__((aligned(8)));
#endif

static uint8_t *g_uploaded_data = NULL;
static size_t g_uploaded_size = 0;

static wasm_module_inst_t wasm_module_inst = NULL;
static waiot_thread_t thread_wamr = {0};

static atomic_bool g_wamr_thread_running = ATOMIC_VAR_INIT(false);
static atomic_bool g_wamr_thread_joined = ATOMIC_VAR_INIT(true);
static waiot_mutex_t g_wamr_thread_mu = {0};
static atomic_bool g_wamr_thread_mu_initialized = ATOMIC_VAR_INIT(false);

static void *g_malloc_func = NULL;
static void *g_realloc_func = NULL;
static void *g_free_func = NULL;

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

static void ensure_thread_primitives_initialized(void)
{
    if (atomic_load(&g_wamr_thread_mu_initialized))
    {
        return;
    }

    if (waiot_mutex_init(&g_wamr_thread_mu) == 0)
    {
        atomic_store(&g_wamr_thread_mu_initialized, true);
    }
    else
    {
        printf("Failed to initialize WAMR mutex.\n");
    }
}

void __attribute__((weak)) waiot_platform_register_extra_natives(void)
{
}

void __attribute__((weak)) waiot_platform_print_memory_info(void)
{
}

void wamr_set_allocator(void *malloc_func, void *realloc_func, void *free_func)
{
    g_malloc_func = malloc_func;
    g_realloc_func = realloc_func;
    g_free_func = free_func;
}

void init_wamr(void)
{
    RuntimeInitArgs init_args;

    ensure_thread_primitives_initialized();

    memset(&init_args, 0, sizeof(RuntimeInitArgs));
#if WASM_ENABLE_GLOBAL_HEAP_POOL == 0
    init_args.mem_alloc_type = Alloc_With_Allocator;
    init_args.mem_alloc_option.allocator.malloc_func = g_malloc_func ? g_malloc_func : (void *)malloc;
    init_args.mem_alloc_option.allocator.realloc_func = g_realloc_func ? g_realloc_func : (void *)realloc;
    init_args.mem_alloc_option.allocator.free_func = g_free_func ? g_free_func : (void *)free;
#else
    init_args.mem_alloc_type = Alloc_With_Pool;
    init_args.mem_alloc_option.pool.heap_buf = g_wamr_global_heap;
    init_args.mem_alloc_option.pool.heap_size = sizeof(g_wamr_global_heap);
#endif

    if (!wasm_runtime_full_init(&init_args))
    {
        printf("Init runtime failed.\n");
        return;
    }

    if (!wasm_runtime_register_natives(
            "wasi:i2c", i2c_syms,
            (uint32_t)(sizeof(i2c_syms) / sizeof(NativeSymbol))))
    {
        printf("Failed to register I2C native symbols.\n");
        return;
    }
    if (!wasm_runtime_register_natives(
            "wasi:digital", gpio_syms,
            (uint32_t)(sizeof(gpio_syms) / sizeof(NativeSymbol))))
    {
        printf("Failed to register GPIO native symbols.\n");
        return;
    }
    if (!wasm_runtime_register_natives(
            "wasi:delay", delay_syms,
            (uint32_t)(sizeof(delay_syms) / sizeof(NativeSymbol))))
    {
        printf("Failed to register Delay native symbols.\n");
        return;
    }
    if (!wasm_runtime_register_natives(
            "wasi:pwm", pwm_syms,
            (uint32_t)(sizeof(pwm_syms) / sizeof(NativeSymbol))))
    {
        printf("Failed to register PWM native symbols.\n");
        return;
    }
    if (!wasm_runtime_register_natives(
            "wasi:wasi_snapshot_preview1", wasi_snapshot_preview1_syms,
            (uint32_t)(sizeof(wasi_snapshot_preview1_syms) / sizeof(NativeSymbol))))
    {
        printf("Failed to register WASI fd_write native symbol.\n");
        return;
    }

    waiot_platform_register_extra_natives();
}

int wamr_set_wasm_binary(const uint8_t *data, size_t size)
{
    ensure_thread_primitives_initialized();

    if (!data || size == 0)
    {
        return -1;
    }

    uint8_t *buf = (uint8_t *)malloc(size);
    if (!buf)
    {
        return -1;
    }
    memcpy(buf, data, size);

    waiot_mutex_lock(&g_wamr_thread_mu);
    if (is_wasm_instance_running())
    {
        waiot_mutex_unlock(&g_wamr_thread_mu);
        free(buf);
        return -2;
    }

    free(g_uploaded_data);
    g_uploaded_data = buf;
    g_uploaded_size = size;
    waiot_mutex_unlock(&g_wamr_thread_mu);
    return 0;
}

static void *run_wamr(void *arg)
{
    (void)arg;
    atomic_store(&g_wamr_thread_running, true);

    unsigned wasm_file_buf_size = 0;
    uint8_t *wasm_file_buf = NULL;
    wasm_module_t wasm_module = NULL;
    char error_buf[128];
    bool if_created_env = false;

    waiot_platform_print_memory_info();

    waiot_mutex_lock(&g_wamr_thread_mu);
    if (!g_uploaded_data || g_uploaded_size == 0)
    {
        printf("No Wasm binary uploaded\n");
        waiot_mutex_unlock(&g_wamr_thread_mu);
        goto done;
    }
    wasm_file_buf_size = (unsigned)g_uploaded_size;
    wasm_file_buf = malloc(g_uploaded_size);
    if (!wasm_file_buf)
    {
        printf("Failed allocating %d bytes for Wasm binary\n",
               (int)g_uploaded_size);
        waiot_mutex_unlock(&g_wamr_thread_mu);
        goto done;
    }
    memcpy(wasm_file_buf, g_uploaded_data, g_uploaded_size);
    waiot_mutex_unlock(&g_wamr_thread_mu);

    if (!(wasm_module = wasm_runtime_load(wasm_file_buf, wasm_file_buf_size,
                                          error_buf, sizeof(error_buf))))
    {
        printf("Error in wasm_runtime_load: %s\n", error_buf);
        goto done;
    }

    if (!(wasm_module_inst =
              wasm_runtime_instantiate(wasm_module, 32 * 1024,
                                       32 * 1024, error_buf,
                                       sizeof(error_buf))))
    {
        printf("Error while instantiating: %s\n", error_buf);
        goto done;
    }

    wasm_exec_env_t exec_env = wasm_runtime_get_exec_env_singleton(wasm_module_inst);
    if (!exec_env)
    {
        printf("Failed to get exec env\n");
        goto done;
    }

    if (!wasm_runtime_init_thread_env())
    {
        printf("Failed to init thread env\n");
        goto done;
    }
    if_created_env = true;

    if (!wasm_application_execute_main(wasm_module_inst, 0, NULL))
    {
        printf("Failed to execute main()\n");
        goto done;
    }

done:
    if (wasm_module_inst)
    {
        wasm_runtime_deinstantiate(wasm_module_inst);
        wasm_module_inst = NULL;
    }
    if (wasm_module)
    {
        wasm_runtime_unload(wasm_module);
    }
    free(wasm_file_buf);
    if (if_created_env)
    {
        wasm_runtime_destroy_thread_env();
    }
    waiot_platform_print_memory_info();
    atomic_store(&g_wamr_thread_running, false);
    return NULL;
}

void stop_current_wasm_instance(void)
{
    ensure_thread_primitives_initialized();

    waiot_thread_t t_for_join = {0};
    bool need_join = false;

    waiot_mutex_lock(&g_wamr_thread_mu);
    if (wasm_module_inst)
    {
        wasm_runtime_terminate(wasm_module_inst);
    }

    if (!atomic_load(&g_wamr_thread_joined))
    {
        t_for_join = thread_wamr;
        need_join = true;
        atomic_store(&g_wamr_thread_joined, true);
    }
    waiot_mutex_unlock(&g_wamr_thread_mu);

    if (need_join)
    {
        waiot_thread_join(&t_for_join);
    }

    waiot_platform_print_memory_info();
}

void launch_new_wasm_instance(void)
{
    ensure_thread_primitives_initialized();

    waiot_mutex_lock(&g_wamr_thread_mu);
    if (!atomic_load(&g_wamr_thread_running) && !atomic_load(&g_wamr_thread_joined))
    {
        waiot_thread_join(&thread_wamr);
        atomic_store(&g_wamr_thread_joined, true);
    }

    atomic_store(&g_wamr_thread_joined, false);
    int res = waiot_thread_create(&thread_wamr, IWASM_MAIN_STACK_SIZE,
                                  run_wamr, NULL);
    if (res != 0)
    {
        printf("Failed to create WAMR thread: %d\n", res);
        atomic_store(&g_wamr_thread_joined, true);
    }

    waiot_mutex_unlock(&g_wamr_thread_mu);
}

bool is_wasm_instance_running(void)
{
    return atomic_load(&g_wamr_thread_running);
}

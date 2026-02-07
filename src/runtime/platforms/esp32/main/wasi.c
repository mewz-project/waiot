#include "wasi.h"

#include "driver/gpio.h"
// #include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_camera.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "pinmap.h"
#include <string.h>

typedef int32_t wasi_bus_handle_t;
typedef int32_t wasi_gpio_handle_t;

#define WASI_MAX_GPIO_PINS 16
#define LOG_RING_BUF_SIZE (8 * 1024)

typedef struct
{
    bool used;
    gpio_num_t pin;
} wasi_gpio_t;

typedef struct
{
    int32_t iov_base;
    int32_t iov_len;
} wasi_iovec_t;

static wasi_gpio_t s_gpio[WASI_MAX_GPIO_PINS];

static uint8_t s_log_ring[LOG_RING_BUF_SIZE];
static size_t s_log_write_pos = 0;
static size_t s_log_filled = 0;

static inline wasi_gpio_t *
get_gpio(wasi_gpio_handle_t h)
{
    if (h < 0 || h >= WASI_MAX_GPIO_PINS)
    {
        return NULL;
    }
    return s_gpio[h].used ? &s_gpio[h] : NULL;
}

//====================== I2C ======================

static bool i2c_initialized = false;
static i2c_master_bus_handle_t s_i2c_bus = NULL;

static struct
{
    int32_t port;
    int32_t sda_gpio;
    int32_t scl_gpio;
    int32_t freq_hz;
    bool has_conf;
} s_i2c_conf = {0};

int32_t waiot_i2c_param_config(wasm_exec_env_t exec_env, int32_t port,
                               int32_t sda_gpio, int32_t scl_gpio,
                               int32_t freq_hz)
{
    ESP_LOGI("wasi_i2c",
             "i2c_param_config(new): port=%d, sda_gpio=%d, scl_gpio=%d, freq_hz=%d",
             port, sda_gpio, scl_gpio, freq_hz);

    s_i2c_conf.port = port;
    s_i2c_conf.sda_gpio = map_pin(sda_gpio);
    s_i2c_conf.scl_gpio = map_pin(scl_gpio);
    s_i2c_conf.freq_hz = freq_hz;
    s_i2c_conf.has_conf = true;
    return 0;
}

int32_t waiot_i2c_driver_install(wasm_exec_env_t exec_env, int32_t port)
{
    if (i2c_initialized)
    {
        ESP_LOGI("wasi_i2c", "i2c_driver_install(new): already initialized");
        return 0;
    }

    if (!s_i2c_conf.has_conf)
    {
        ESP_LOGE("wasi_i2c", "i2c_driver_install(new): param_config not called");
        return -1;
    }

    if (s_i2c_conf.port != port)
    {
        ESP_LOGW("wasi_i2c",
                 "i2c_driver_install(new): port mismatch (param_config=%d, install=%d). Using install port.",
                 (int)s_i2c_conf.port, (int)port);
        s_i2c_conf.port = port;
    }

    ESP_LOGI("wasi_i2c",
             "i2c_driver_install(new): port=%d, sda=%d, scl=%d",
             (int)s_i2c_conf.port, (int)s_i2c_conf.sda_gpio, (int)s_i2c_conf.scl_gpio);

    // const i2c_master_bus_config_t bus_cfg = {
    //     .i2c_port = (i2c_port_t)s_i2c_conf.port,
    //     .sda_io_num = (gpio_num_t)s_i2c_conf.sda_gpio,
    //     .scl_io_num = (gpio_num_t)s_i2c_conf.scl_gpio,
    //     .clk_source = I2C_CLK_SRC_DEFAULT,
    // };
    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = 1,
        .sda_io_num = 12,
        .scl_io_num = 11,
        .clk_source = I2C_CLK_SRC_DEFAULT,
    };

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
    if (err != ESP_OK)
    {
        ESP_LOGE("wasi_i2c", "i2c_new_master_bus failed: 0x%x", (unsigned)err);
        s_i2c_bus = NULL;
        return -1;
    }

    i2c_initialized = true;
    return 0;
}

int32_t waiot_i2c_master_write(wasm_exec_env_t exec_env, int32_t port,
                               int32_t addr, int32_t write_buff_ptr_idx, int32_t write_size, int32_t ticks_to_wait){
                               return 0;
                               }
int32_t waiot_i2c_master_read(wasm_exec_env_t exec_env, int32_t port,
                              int32_t addr, int32_t read_buff_ptr_idx, int32_t read_size, int32_t ticks_to_wait){
    return 0;
}
/*
int32_t waiot_i2c_param_config(wasm_exec_env_t exec_env, int32_t port,
                               int32_t sda_gpio, int32_t scl_gpio,
                               int32_t freq_hz)
{
    ESP_LOGI("wasi_i2c", "i2c_param_config: port=%d, sda_gpio=%d, scl_gpio=%d, freq_hz=%d",
             port, sda_gpio, scl_gpio, freq_hz);
    i2c_config_t conf = {.mode = I2C_MODE_MASTER,
                         .sda_io_num = (gpio_num_t)map_pin(sda_gpio),
                         .scl_io_num = (gpio_num_t)map_pin(scl_gpio),
                         .sda_pullup_en = GPIO_PULLUP_ENABLE,
                         .scl_pullup_en = GPIO_PULLUP_ENABLE,
                         .master.clk_speed = (uint32_t)freq_hz};
    esp_err_t err;
    err = i2c_param_config((i2c_port_t)port, &conf);
    if (err != ESP_OK)
    {
        ESP_LOGE("wasi_i2c", "i2c_param_config: i2c_param_config failed: %d", err);
        return -1;
    }
    return 0;
}

int32_t waiot_i2c_driver_install(wasm_exec_env_t exec_env, int32_t port)
{
    if (i2c_initialized)
    {
        ESP_LOGI("wasi_i2c", "i2c_driver_install: already initialized");
        return 0;
    }
    ESP_LOGI("wasi_i2c", "i2c_driver_install: port=%d", port);
    esp_err_t err;
    err = i2c_driver_install((i2c_port_t)port, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE("wasi_i2c", "i2c_driver_install: i2c_driver_install failed: %d", err);
        return -1;
    }
    i2c_initialized = true;
    return 0;
}

int32_t waiot_i2c_master_write(wasm_exec_env_t exec_env, int32_t port,
                               int32_t addr, int32_t write_buff_ptr_idx, int32_t write_size, int32_t ticks_to_wait)
{
    // Convert to native pointer
    ESP_LOGI("wasi_i2c", "i2c_master_write: port=%d, addr=0x%02x, write_buff_ptr_idx=0x%08x, write_size=%d, ticks_to_wait=%d",
             port, addr, write_buff_ptr_idx, write_size, ticks_to_wait);
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    if (!instance)
    {
        ESP_LOGE("wasi_i2c", "i2c_master_write: Failed to get module instance.");
        return -1;
    }

    if (!wasm_runtime_validate_app_addr(instance, write_buff_ptr_idx, write_size))
    {
        ESP_LOGE("wasi_i2c", "waiot_i2c_master_write: Invalid write buffer address.");
        return -1;
    }

    char *write_buff = (char *)wasm_runtime_addr_app_to_native(instance, write_buff_ptr_idx);
    esp_err_t err = i2c_master_write_to_device((i2c_port_t)port, (uint8_t)addr,
                                               (const uint8_t *)write_buff,
                                               (size_t)write_size,
                                               (TickType_t)ticks_to_wait);
    switch (err)
    {
    case ESP_OK:
        break;
    case ESP_ERR_INVALID_ARG:
        ESP_LOGE("wasi_i2c", " Invalid argument");
        break;
    case ESP_FAIL:
        ESP_LOGE("wasi_i2c", " Operation failed");
        break;
    case ESP_ERR_INVALID_STATE:
        ESP_LOGE("wasi_i2c", " I2C driver not installed or not in master mode");
        break;
    case ESP_ERR_TIMEOUT:
        ESP_LOGE("wasi_i2c", " Operation timeout");
        break;
    default:
        ESP_LOGE("wasi_i2c", " Unknown error");
        break;
    }
    return (err == ESP_OK) ? 0 : -1;
}

int32_t waiot_i2c_master_read(wasm_exec_env_t exec_env, int32_t port,
                              int32_t addr, int32_t read_buff_ptr_idx, int32_t read_size, int32_t ticks_to_wait)
{
    ESP_LOGI("wasi_i2c", "i2c_master_read: port=%d, addr=0x%02x, read_buff_ptr_idx=0x%08x, read_size=%d, ticks_to_wait=%d",
             port, addr, read_buff_ptr_idx, read_size, ticks_to_wait);
    // Convert to native pointer
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    if (!instance)
    {
        ESP_LOGE("wasi_i2c", "i2c_master_read: Failed to get module instance.");
        return -1;
    }

    if (!wasm_runtime_validate_app_addr(instance, read_buff_ptr_idx, read_size))
    {
        ESP_LOGE("wasi_i2c", "waiot_i2c_master_read: Invalid read buffer address.");
        return -1;
    }

    char *read_buff = (char *)wasm_runtime_addr_app_to_native(instance, read_buff_ptr_idx);
    esp_err_t err = i2c_master_read_from_device((i2c_port_t)port, (uint8_t)addr,
                                                (uint8_t *)read_buff,
                                                (size_t)read_size,
                                                (TickType_t)ticks_to_wait);
    return (err == ESP_OK) ? 0 : -1;
}
*/

//====================== GPIO ======================

int32_t
gpio_set_pin_mode(wasm_exec_env_t exec_env, int32_t pin, int32_t dir)
{
    int32_t hw_pin = map_pin(pin);
    gpio_num_t p = (gpio_num_t)hw_pin;
    gpio_reset_pin(p);
    gpio_mode_t mode = (dir == 0) ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT;
    gpio_pull_mode_t pm = GPIO_FLOATING;
    if (gpio_set_direction(p, mode) != ESP_OK)
    {
        return -1;
    }
    if (gpio_set_pull_mode(p, pm) != ESP_OK)
    {
        return -1;
    }
    return 0;
}

int32_t
gpio_read(wasm_exec_env_t exec_env, int32_t pin)
{
    int32_t hw_pin = map_pin(pin);
    ESP_LOGI("WASI_GPIO", "gpio_read: virtual=%d mapped=%d", pin, hw_pin);
    return gpio_get_level(hw_pin);
}

int32_t
gpio_write(wasm_exec_env_t exec_env, int32_t pin, int32_t value)
{
    int32_t hw_pin = map_pin(pin);
    ESP_LOGI("WASI_GPIO", "gpio_write: virtual=%d mapped=%d value=%d", pin, hw_pin, value);
    return (gpio_set_level(hw_pin, value ? 1 : 0) == ESP_OK) ? 0 : -1;
}

// ====================== Delay ======================

// Delay
// Since ESP-IDF does not provide ns-level delay, we round up to us.
int32_t delay_ns(wasm_exec_env_t exec_env, int32_t ns)
{
    if (ns <= 0)
    {
        return 0;
    }
    uint32_t ns_saturating_add = (ns >= INT32_MAX - 999999) ? INT32_MAX : ns + 999999;
    uint32_t ms = ns_saturating_add / 1000000;
    vTaskDelay(pdMS_TO_TICKS(ms));
    return 0;
}

// ====================== LEDC ======================

int32_t pwm_init(wasm_exec_env_t exec_env, int32_t pin, int32_t channel, int32_t freq, int32_t resolution, int32_t speed_mode)
{
    int32_t hw_pin = map_pin((int32_t)pin);
    ESP_LOGI("WASI_LEDC", "pwm_init: pin=%d mapped=%d, channel=%d, freq=%d, resolution=%d, speed_mode=%d",
             pin, hw_pin, channel, freq, resolution, speed_mode);

    // --- Configure LEDC timer ---
    ledc_timer_config_t tcfg = {
        .speed_mode = (ledc_mode_t)speed_mode,
        .duty_resolution = (ledc_timer_bit_t)resolution,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = freq,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&tcfg));

    // --- Configure LEDC channel ---
    ledc_channel_config_t ccfg = {
        .gpio_num = hw_pin,
        .speed_mode = (ledc_mode_t)speed_mode,
        .channel = channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0, // start silent
        .hpoint = 0,
        .flags.output_invert = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ccfg));
    return 0;
}

int32_t pwm_set_duty(wasm_exec_env_t exec_env, int32_t channel, int32_t duty, int32_t speed_mode)
{
    ESP_LOGI("WASI_LEDC", "pwm_set_duty: channel=%d, duty=%d, speed_mode=%d",
             channel, duty, speed_mode);
    ESP_ERROR_CHECK(ledc_set_duty((ledc_mode_t)speed_mode, channel, duty));
    return 0;
}

int32_t pwm_update_duty(wasm_exec_env_t exec_env, int32_t channel, int32_t speed_mode)
{
    ESP_LOGI("WASI_LEDC", "pwm_update_duty: channel=%d, speed_mode=%d",
             channel, speed_mode);
    ESP_ERROR_CHECK(ledc_update_duty((ledc_mode_t)speed_mode, channel));
    return 0;
}

int32_t pwm_set_frequency(wasm_exec_env_t exec_env, int32_t channel, int32_t freq, int32_t speed_mode)
{
    ESP_LOGI("WASI_LEDC", "pwm_set_frequency: channel=%d, freq=%d, speed_mode=%d",
             channel, freq, speed_mode);
    ESP_ERROR_CHECK(ledc_set_freq((ledc_mode_t)speed_mode, channel, freq));
    return 0;
}

// ====================== Logging (fd_write) ======================

static void
log_ring_append(const uint8_t *data, size_t len)
{
    while (len > 0)
    {
        size_t to_end = LOG_RING_BUF_SIZE - s_log_write_pos;
        size_t chunk = len < to_end ? len : to_end;
        memcpy(s_log_ring + s_log_write_pos, data, chunk);
        s_log_write_pos = (s_log_write_pos + chunk) % LOG_RING_BUF_SIZE;
        if (s_log_filled + chunk >= LOG_RING_BUF_SIZE)
        {
            s_log_filled = LOG_RING_BUF_SIZE;
        }
        else
        {
            s_log_filled += chunk;
        }
        data += chunk;
        len -= chunk;
    }
}

int32_t
fd_write(wasm_exec_env_t exec_env, int32_t fd, int32_t buf_iovec_addr,
         int32_t vec_len, int32_t size_addr)
{
    ESP_LOGI("wasi_fd", "fd_write: fd=%d, buf_iovec_addr=0x%08x, vec_len=%d, size_addr=0x%08x",
             fd, buf_iovec_addr, vec_len, size_addr);
    // only stdout is supported
    if (fd != 1)
    {
        return -1; 
    }

    wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
    if (!inst)
    {
        return -2;
    }

    if (vec_len < 0)
    {
        return -3;
    }

    uint32_t iovecs_size = (uint32_t)vec_len * (uint32_t)sizeof(wasi_iovec_t);
    if (!wasm_runtime_validate_app_addr(inst, buf_iovec_addr, iovecs_size))
    {
        return -4;
    }
    wasi_iovec_t *iovecs = (wasi_iovec_t *)
        wasm_runtime_addr_app_to_native(inst, buf_iovec_addr);
    if (!iovecs)
    {
        return -5;
    }

    uint32_t total_written = 0;
    for (int32_t i = 0; i < vec_len; i++)
    {
        int32_t base = iovecs[i].iov_base;
        int32_t len = iovecs[i].iov_len;
        if (len < 0)
        {
            return -6;
        }
        if (len == 0)
        {
            continue;
        }
        if (!wasm_runtime_validate_app_addr(inst, base, (uint32_t)len))
        {
            return -7;
        }
        const uint8_t *ptr = (const uint8_t *)
            wasm_runtime_addr_app_to_native(inst, base);
        if (!ptr)
        {
            return -8;
        }
        log_ring_append(ptr, (size_t)len);
        total_written += (uint32_t)len;
    }

    if (!wasm_runtime_validate_app_addr(inst, size_addr, sizeof(uint32_t)))
    {
        return -9;
    }
    uint32_t *nwritten_ptr = (uint32_t *)
        wasm_runtime_addr_app_to_native(inst, size_addr);
    if (!nwritten_ptr)
    {
        return -10;
    }
    *nwritten_ptr = total_written;

    return 0;
}

// Read-only helpers to expose ring buffer for HTTP serving
int32_t log_get_filled(void)
{
    return (int32_t)s_log_filled;
}

int32_t log_read_from_oldest(uint32_t offset, uint8_t *out, uint32_t len)
{
    if (offset > s_log_filled)
    {
        return -1;
    }
    uint32_t can_read = (uint32_t)s_log_filled - offset;
    if (len > can_read)
    {
        len = can_read;
    }
    if (len == 0)
    {
        return 0;
    }
    uint32_t oldest = (uint32_t)((s_log_write_pos + LOG_RING_BUF_SIZE - s_log_filled) % LOG_RING_BUF_SIZE);
    uint32_t start = (oldest + offset) % LOG_RING_BUF_SIZE;
    uint32_t to_end = LOG_RING_BUF_SIZE - start;
    uint32_t chunk1 = len < to_end ? len : to_end;
    memcpy(out, s_log_ring + start, chunk1);
    if (chunk1 < len)
    {
        memcpy(out + chunk1, s_log_ring, len - chunk1);
    }
    return (int32_t)len;
}

// ====================== HTTP client ======================
#define MAX_HTTP_HANDLES 8

typedef enum
{
    HTTP_M_GET = 0,
    HTTP_M_POST = 1,
    HTTP_M_PUT = 2,
    HTTP_M_DEL = 3,
} wasm_http_method_t;

typedef struct
{
    bool used;
    esp_http_client_handle_t client;
} http_slot_t;

static http_slot_t g_http_slots[MAX_HTTP_HANDLES];
static SemaphoreHandle_t g_http_lock;

void init_http_client(void)
{
    g_http_lock = xSemaphoreCreateMutex();
}

static int alloc_http_handle(esp_http_client_handle_t client)
{
    for (int i = 0; i < MAX_HTTP_HANDLES; i++)
    {
        if (!g_http_slots[i].used)
        {
            g_http_slots[i].used = true;
            g_http_slots[i].client = client;
            return i;
        }
    }
    xSemaphoreGive(g_http_lock);
    return -EMFILE;
}

static esp_http_client_handle_t get_client(int handle)
{
    if (handle < 0 || handle >= MAX_HTTP_HANDLES)
        return NULL;
    if (!g_http_slots[handle].used)
        return NULL;
    return g_http_slots[handle].client;
}

int32_t http_init(wasm_exec_env_t exec_env, int32_t url_ptr,
                  int32_t url_len,
                  int32_t timeout_ms) {
    wasm_module_inst_t module = wasm_runtime_get_module_inst(exec_env);

    if (!wasm_runtime_validate_app_addr(module, url_ptr, url_len))
        return -EFAULT;

    char url[256];
    if (url_len >= sizeof(url)) {
        ESP_LOGE("WASI_HTTP", "http_init: URL too long: %d bytes", url_len);
        return -ENAMETOOLONG;   
    }

    memcpy(url,
           wasm_runtime_addr_app_to_native(module, url_ptr),
           url_len);
    url[url_len] = '\0';

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = timeout_ms,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE("WASI_HTTP", "http_init: esp_http_client_init failed");
        return -ENOMEM;
    }
    return alloc_http_handle(client);
}

int32_t http_open(wasm_exec_env_t exec_env,
                int32_t handle,
                int32_t method,
                int32_t content_len)
{
    wasm_module_inst_t module = wasm_runtime_get_module_inst(exec_env);

    esp_http_client_handle_t client = get_client(handle);
    if (!client)
    {
        ESP_LOGE("WASI_HTTP", "http_open: invalid handle: %d", handle);
        return -EBADF;
    }

    switch (method)
    {
    case HTTP_M_GET:
        esp_http_client_set_method(client, HTTP_METHOD_GET);
        break;
    case HTTP_M_POST:
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        break;
    case HTTP_M_PUT:
        esp_http_client_set_method(client, HTTP_METHOD_PUT);
        break;
    case HTTP_M_DEL:
        esp_http_client_set_method(client, HTTP_METHOD_DELETE);
        break;
    default:
        esp_http_client_cleanup(client);
        ESP_LOGE("WASI_HTTP", "http_open: invalid method: %d", method);
        return -EINVAL;
    }

    esp_err_t err = esp_http_client_open(client, content_len);
    if (err != ESP_OK)
    {
        esp_http_client_cleanup(client);
        ESP_LOGE("WASI_HTTP", "http_open: esp_http_client_open failed: %s",
                 esp_err_to_name(err));
        return -ECONNREFUSED;
    }

    return 0;
}

int32_t http_set_header(wasm_exec_env_t exec_env,
                        int32_t handle,
                        int32_t k_ptr, int32_t k_len,
                        int32_t v_ptr, int32_t v_len)
{
    wasm_module_inst_t module = wasm_runtime_get_module_inst(exec_env);
    esp_http_client_handle_t client = get_client(handle);
    if (!client) {
        return -EBADF;
    }

    if (!wasm_runtime_validate_app_addr(module, k_ptr, k_len) ||
        !wasm_runtime_validate_app_addr(module, v_ptr, v_len)) {
        return -EFAULT;
    }

    char key[64], val[128];
    if (k_len >= sizeof(key) || v_len >= sizeof(val)) {
        return -ENAMETOOLONG;
    }

    memcpy(key, wasm_runtime_addr_app_to_native(module, k_ptr), k_len);
    memcpy(val, wasm_runtime_addr_app_to_native(module, v_ptr), v_len);
    key[k_len] = '\0';
    val[v_len] = '\0';

    esp_http_client_set_header(client, key, val);
    return 0;
}

int32_t http_fetch_headers(wasm_exec_env_t exec_env, int32_t handle)
{
    esp_http_client_handle_t client = get_client(handle);
    if (!client)
    {
        return -EBADF;
    }

    int64_t content_length = esp_http_client_fetch_headers(client);

    int status = esp_http_client_get_status_code(client);

    ESP_LOGI("WASI_HTTP", "http_fetch_headers: handle=%d => content_length=%lld, status=%d",
             handle, content_length, status);

    if (content_length < -1)
    {
        return -EIO;
    }
    if (content_length > INT32_MAX)
    {
        ESP_LOGW("WASI_HTTP", "http_fetch_headers: content_length too large: %lld",
                 content_length);
        return INT32_MAX;
    }
    return (int32_t)content_length;
}

int32_t http_write(wasm_exec_env_t exec_env,
                       int32_t handle,
                       int32_t buf_ptr,
                       int32_t buf_len)
{
    wasm_module_inst_t module = wasm_runtime_get_module_inst(exec_env);
    esp_http_client_handle_t client = get_client(handle);
    if (!client)
    {
        return -EBADF;
    }

    if (!wasm_runtime_validate_app_addr(module, buf_ptr, buf_len))
    {
        return -EFAULT;
    }

    const char *buf =
        wasm_runtime_addr_app_to_native(module, buf_ptr);

    int ret = esp_http_client_write(client, buf, buf_len);
    return (ret >= 0) ? ret : -EIO;
}

int32_t http_read(wasm_exec_env_t exec_env,
                    int32_t handle,
                    int32_t buf_ptr,
                    int32_t buf_len)
{
    wasm_module_inst_t module = wasm_runtime_get_module_inst(exec_env);
    esp_http_client_handle_t client = get_client(handle);
    if (!client)
    {
        return -EBADF;
    }

    if (!wasm_runtime_validate_app_addr(module, buf_ptr, buf_len))
    {
        return -EFAULT;
    }

    char *buf =
        wasm_runtime_addr_app_to_native(module, buf_ptr);

    int ret = esp_http_client_read(client, buf, buf_len);
    ESP_LOGI("WASI_HTTP", "http_read: handle=%d, buf_ptr=0x%08x, buf_len=%d => read=%d",
                handle, buf_ptr, buf_len, ret);
    ESP_LOGI("WASI_HTTP", "Data: %.*s", ret, buf);
    return (ret >= 0) ? ret : -EIO;
}

int32_t http_status(wasm_exec_env_t exec_env, int32_t handle)
{
    esp_http_client_handle_t client = get_client(handle);
    if (!client)
    {
        return -EBADF;
    }

    return esp_http_client_get_status_code(client);
}

int32_t http_close(wasm_exec_env_t exec_env, int32_t handle)
{
    if (handle < 0 || handle >= MAX_HTTP_HANDLES)
    {
        return -EBADF;
    }

    if (g_http_lock == NULL)
    {
        return -EBADF;
    }
    xSemaphoreTake(g_http_lock, portMAX_DELAY);

    if (!g_http_slots[handle].used)
    {
        xSemaphoreGive(g_http_lock);
        return -EBADF;
    }

    esp_http_client_handle_t client = g_http_slots[handle].client;
    g_http_slots[handle].used = false;
    g_http_slots[handle].client = NULL;

    xSemaphoreGive(g_http_lock);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return 0;
}

// ====================== Camera ======================

enum {
    CAMERA_TYPE_OV2640 = 0,
    CAMERA_TYPE_GC0308 = 1,
};

static bool has_camera_initialized = false;
static int current_pixel_format = -1;
static int current_frame_size = -1;
static int current_jpeg_quality = -1;

static bool has_psram(void)
{
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0;
}

int32_t if_camera_config_changed(wasm_exec_env_t exec_env, int pixel_format, int frame_size, int jpeg_quality)
{
    if (has_camera_initialized &&
        current_pixel_format == pixel_format &&
        current_frame_size == frame_size &&
        current_jpeg_quality == jpeg_quality)
    {
        return 0; // No change
    }
    return 1; // Config changed
}
#include "bsp/m5stack_core_s3.h"
int32_t camera_init(wasm_exec_env_t exec_env, int camera_device_type, int pixel_format, int frame_size, int jpeg_quality)
{
    if (has_camera_initialized)
    {
        esp_camera_deinit();
    }

    ESP_LOGI("wasi", "camera_init: pixel_format=%d, frame_size=%d, jpeg_quality=%d",
             pixel_format, frame_size, jpeg_quality);

    // int ret = bsp_i2c_init();
    // if (ret != 0)
    // {
    //     ESP_LOGE("wasi", "bsp_i2c_init failed: %d", ret);
    //     return ESP_FAIL;
    // }

    has_camera_initialized = true;
    current_pixel_format = pixel_format;
    current_frame_size = frame_size;
    current_jpeg_quality = jpeg_quality;
    camera_config_t config = {0};

    switch (camera_device_type)
    {
    case CAMERA_TYPE_OV2640:
        ESP_LOGI("wasi", "Camera type: OV2640");
        config.pin_pwdn = -1;
        config.pin_reset = -1;
        config.pin_xclk = 4;
        config.pin_sccb_sda = 18;
        config.pin_sccb_scl = 23;
        config.pin_d7 = 36;
        config.pin_d6 = 37;
        config.pin_d5 = 38;
        config.pin_d4 = 39;
        config.pin_d3 = 35;
        config.pin_d2 = 14;
        config.pin_d1 = 13;
        config.pin_d0 = 34;
        config.pin_vsync = 5;
        config.pin_href = 27;
        config.pin_pclk = 25;
        config.xclk_freq_hz = 20000000;
        config.ledc_timer = LEDC_TIMER_0;
        config.ledc_channel = LEDC_CHANNEL_0;
        gpio_set_direction((gpio_num_t)13, GPIO_MODE_INPUT);
        gpio_set_direction((gpio_num_t)14, GPIO_MODE_INPUT);
        break;
    case CAMERA_TYPE_GC0308:
        ESP_LOGI("wasi", "Camera type: GC0308");
        config.pin_pwdn = GPIO_NUM_NC;
        config.pin_reset = GPIO_NUM_NC;
        config.pin_xclk = GPIO_NUM_NC;
        config.pin_sccb_sda = GPIO_NUM_NC;
        config.pin_sccb_scl = GPIO_NUM_NC;
        config.pin_d7 = 47;
        config.pin_d6 = 48;
        config.pin_d5 = 16;
        config.pin_d4 = 15;
        config.pin_d3 = 42;
        config.pin_d2 = 41;
        config.pin_d1 = 40;
        config.pin_d0 = 39;
        config.pin_vsync = 46;
        config.pin_href = 38;
        config.pin_pclk = 45;
        config.xclk_freq_hz = 10000000;
        config.ledc_timer = LEDC_TIMER_0;
        config.ledc_channel = LEDC_CHANNEL_0;
        config.sccb_i2c_port = 1;
        break;
    default:
        ESP_LOGE("wasi", "Unsupported camera type: %d", camera_device_type);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    config.pixel_format = (pixformat_t)pixel_format;
    config.frame_size = (framesize_t)frame_size;
    config.jpeg_quality = jpeg_quality;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    // Try to use PSRAM for frame buffer if available
        /*
        if (has_psram())
        {
            config.fb_location = CAMERA_FB_IN_PSRAM;
        }
        else
        {
            config.fb_location = CAMERA_FB_IN_DRAM;
            config.fb_count = 1;
            ESP_LOGW("wasi", "PSRAM not found; lowering fb_count to 1");
        }
        */

    esp_log_level_set("camera", ESP_LOG_DEBUG);
    esp_log_level_set("sccb", ESP_LOG_DEBUG);

    esp_err_t err = esp_camera_init(&config);

    if (err != ESP_OK)
    {
        ESP_LOGE("wasi", "Camera init failed: 0x%x", err);
        return err;
    }
    ESP_LOGI("wasi", "Camera initialized with format=%d, size=%d, quality=%d",
             config.pixel_format, config.frame_size, config.jpeg_quality);

    return ESP_OK;
}

int32_t camera_get(wasm_exec_env_t exec_env, int32_t buf_ptr, int32_t buf_size)
{
    ESP_LOGI("wasi", "camera_get...");
    if (!has_camera_initialized)
    {
        ESP_LOGE("wasi", "Camera not initialized");
        return -1;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        ESP_LOGE("wasi", "esp_camera_fb_get failed");
        ESP_LOGI("wasi", "free heap=%u free DMA=%u largest DMA=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));

        return -1;
    }

    ESP_LOGI("wasi", "Captured: size=%d bytes", fb->len);

    // Check buffer size
    if (buf_size < (int32_t)fb->len)
    {
        ESP_LOGE("wasi", "Buffer too small: %d < %d", buf_size, fb->len);
        esp_camera_fb_return(fb);
        return -1;
    }

    // Convert wasm pointer to native pointer
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    if (!instance)
    {
        ESP_LOGE("wasi", "camera_get: Failed to get module instance.");
        esp_camera_fb_return(fb);
        return -1;
    }

    if (!wasm_runtime_validate_app_addr(instance, buf_ptr, (uint32_t)fb->len))
    {
        ESP_LOGE("wasi", "camera_get: Invalid buffer address.");
        esp_camera_fb_return(fb);
        return -1;
    }

    uint8_t *buf = (uint8_t *)wasm_runtime_addr_app_to_native(instance, buf_ptr);
    if (!buf)
    {
        ESP_LOGE("wasi", "camera_get: Failed to get native buffer pointer.");
        esp_camera_fb_return(fb);
        return -1;
    }

    // Copy frame buffer to user buffer
    memcpy(buf, fb->buf, fb->len);
    const int len = fb->len;

    // ESP_LOGI("wasi", "  %02x %02x %02x %02x %02x %02x %02x %02x ... %02x %02x",
    //          buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[len - 2], buf[len - 1]);
    // ESP_LOGI("wasi", "  %02x %02x %02x %02x %02x %02x %02x %02x ... %02x %02x",
    //          fb->buf[0], fb->buf[1], fb->buf[2], fb->buf[3], fb->buf[4], fb->buf[5], fb->buf[6], fb->buf[7], fb->buf[len - 2], fb->buf[len - 1]);

    esp_camera_fb_return(fb);
    ESP_LOGI("wasi", "camera_get: copied %d bytes to wasm buffer at 0x%08x",
             fb->len, buf_ptr);
    return len;
}
#include "wasi.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
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
    ESP_LOGI("wasi_i2c", "i2c_driver_install: port=%d", port);
    esp_err_t err;
    err = i2c_driver_install((i2c_port_t)port, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE("wasi_i2c", "i2c_driver_install: i2c_driver_install failed: %d", err);
        return -1;
    }
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

int32_t http_open(wasm_exec_env_t exec_env,
                  int32_t method,
                  int32_t url_ptr,
                  int32_t url_len,
                  int32_t timeout_ms,
                  int32_t content_len)
{
    wasm_module_inst_t module = wasm_runtime_get_module_inst(exec_env);

    if (!wasm_runtime_validate_app_addr(module, url_ptr, url_len))
        return -EFAULT;

    char url[256];
    if (url_len >= sizeof(url)) {
        ESP_LOGE("WASI_HTTP", "http_open: URL too long: %d bytes", url_len);
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
        ESP_LOGE("WASI_HTTP", "http_open: esp_http_client_init failed");
        return -ENOMEM;
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

    ESP_LOGI("WASI_HTTP", "http_open: method=%d, url=%s, timeout_ms=%d, content_len=%d",
             method, url, timeout_ms, content_len);

    esp_err_t err = esp_http_client_open(client, content_len);
    if (err != ESP_OK)
    {
        esp_http_client_cleanup(client);
        ESP_LOGE("WASI_HTTP", "http_open: esp_http_client_open failed: %s",
                 esp_err_to_name(err));
        return -ECONNREFUSED;
    }

    return alloc_http_handle(client);
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

#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 4
#define SIOD_GPIO_NUM 18
#define SIOC_GPIO_NUM 23

#define Y9_GPIO_NUM 36
#define Y8_GPIO_NUM 37
#define Y7_GPIO_NUM 38
#define Y6_GPIO_NUM 39
#define Y5_GPIO_NUM 35
#define Y4_GPIO_NUM 14
#define Y3_GPIO_NUM 13
#define Y2_GPIO_NUM 34
#define VSYNC_GPIO_NUM 5
#define HREF_GPIO_NUM 27
#define PCLK_GPIO_NUM 25

static bool has_camera_initialized = false;

static bool has_psram(void)
{
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0;
}

int32_t camera_init(wasm_exec_env_t exec_env)
{
    if (has_camera_initialized)
    {
        return ESP_OK;
    }
    has_camera_initialized = true;
    gpio_config_t conf;
    conf.mode = GPIO_MODE_INPUT;
    conf.pull_up_en = GPIO_PULLUP_ENABLE;
    conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    conf.intr_type = GPIO_INTR_DISABLE;
    conf.pin_bit_mask = 1LL << 13;
    gpio_config(&conf);
    conf.pin_bit_mask = 1LL << 14;
    gpio_config(&conf);

    camera_config_t config = {
        .pin_pwdn = PWDN_GPIO_NUM,
        .pin_reset = RESET_GPIO_NUM,
        .pin_xclk = XCLK_GPIO_NUM,
        .pin_sccb_sda = SIOD_GPIO_NUM,
        .pin_sccb_scl = SIOC_GPIO_NUM,
        .pin_d7 = Y9_GPIO_NUM,
        .pin_d6 = Y8_GPIO_NUM,
        .pin_d5 = Y7_GPIO_NUM,
        .pin_d4 = Y6_GPIO_NUM,
        .pin_d3 = Y5_GPIO_NUM,
        .pin_d2 = Y4_GPIO_NUM,
        .pin_d1 = Y3_GPIO_NUM,
        .pin_d0 = Y2_GPIO_NUM,

        .pin_vsync = VSYNC_GPIO_NUM,
        .pin_href = HREF_GPIO_NUM,
        .pin_pclk = PCLK_GPIO_NUM,

        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_GRAYSCALE,
        .frame_size = FRAMESIZE_96X96,
        .jpeg_quality = 8,
        .fb_count = 1,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY
    };

    // Try to use PSRAM for frame buffer if available
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

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        ESP_LOGE("wasi", "Camera init failed: 0x%x", err);
        return err;
    }

    return ESP_OK;
}

int32_t camera_get(wasm_exec_env_t exec_env, int32_t buf_ptr, int32_t buf_size)
{
    ESP_LOGI("wasi", "camera_get...");
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        ESP_LOGE("wasi", "Capture failed");
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
    esp_camera_fb_return(fb);
    return fb->len;
}
#include "wasi.h"

#include "log.h"
#include "pinmap.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

typedef int32_t wasi_bus_handle_t;
typedef int32_t wasi_gpio_handle_t;

#define WASI_MAX_GPIO_PINS 16

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

static inline wasi_gpio_t *
get_gpio(wasi_gpio_handle_t h)
{
    if (h < 0 || h >= WASI_MAX_GPIO_PINS)
    {
        return NULL;
    }
    return s_gpio[h].used ? &s_gpio[h] : NULL;
}

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
    esp_err_t err = i2c_param_config((i2c_port_t)port, &conf);
    if (err != ESP_OK)
    {
        ESP_LOGE("wasi_i2c", "i2c_param_config failed: %d", err);
        return -1;
    }
    return 0;
}

int32_t waiot_i2c_driver_install(wasm_exec_env_t exec_env, int32_t port)
{
    ESP_LOGI("wasi_i2c", "i2c_driver_install: port=%d", port);
    esp_err_t err = i2c_driver_install((i2c_port_t)port, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE("wasi_i2c", "i2c_driver_install failed: %d", err);
        return -1;
    }
    return 0;
}

int32_t waiot_i2c_master_write(wasm_exec_env_t exec_env, int32_t port,
                               int32_t addr, int32_t write_buff_ptr_idx,
                               int32_t write_size, int32_t ticks_to_wait)
{
    ESP_LOGI("wasi_i2c", "i2c_master_write: port=%d, addr=0x%02x, write_buff_ptr_idx=0x%08x, write_size=%d, ticks_to_wait=%d",
             port, addr, write_buff_ptr_idx, write_size, ticks_to_wait);
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    if (!instance)
    {
        ESP_LOGE("wasi_i2c", "Failed to get module instance.");
        return -1;
    }

    if (!wasm_runtime_validate_app_addr(instance, write_buff_ptr_idx, write_size))
    {
        ESP_LOGE("wasi_i2c", "Invalid write buffer address.");
        return -1;
    }

    char *write_buff = (char *)wasm_runtime_addr_app_to_native(instance, write_buff_ptr_idx);
    esp_err_t err = i2c_master_write_to_device((i2c_port_t)port, (uint8_t)addr,
                                               (const uint8_t *)write_buff,
                                               (size_t)write_size,
                                               (TickType_t)ticks_to_wait);
    return (err == ESP_OK) ? 0 : -1;
}

int32_t waiot_i2c_master_read(wasm_exec_env_t exec_env, int32_t port,
                              int32_t addr, int32_t read_buff_ptr_idx,
                              int32_t read_size, int32_t ticks_to_wait)
{
    ESP_LOGI("wasi_i2c", "i2c_master_read: port=%d, addr=0x%02x, read_buff_ptr_idx=0x%08x, read_size=%d, ticks_to_wait=%d",
             port, addr, read_buff_ptr_idx, read_size, ticks_to_wait);
    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    if (!instance)
    {
        ESP_LOGE("wasi_i2c", "Failed to get module instance.");
        return -1;
    }

    if (!wasm_runtime_validate_app_addr(instance, read_buff_ptr_idx, read_size))
    {
        ESP_LOGE("wasi_i2c", "Invalid read buffer address.");
        return -1;
    }

    char *read_buff = (char *)wasm_runtime_addr_app_to_native(instance, read_buff_ptr_idx);
    esp_err_t err = i2c_master_read_from_device((i2c_port_t)port, (uint8_t)addr,
                                                (uint8_t *)read_buff,
                                                (size_t)read_size,
                                                (TickType_t)ticks_to_wait);
    return (err == ESP_OK) ? 0 : -1;
}

int32_t waiot_i2c_master_write_read(wasm_exec_env_t exec_env, int32_t port,
                                    int32_t addr,
                                    int32_t write_buff_ptr_idx,
                                    int32_t write_size,
                                    int32_t read_buff_ptr_idx,
                                    int32_t read_size,
                                    int32_t ticks_to_wait)
{
    ESP_LOGI("wasi_i2c", "i2c_master_write_read: port=%d, addr=0x%02x, "
             "write_buff_ptr_idx=0x%08x, write_size=%d, "
             "read_buff_ptr_idx=0x%08x, read_size=%d, ticks_to_wait=%d",
             port, addr, write_buff_ptr_idx, write_size,
             read_buff_ptr_idx, read_size, ticks_to_wait);

    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    if (!instance)
    {
        ESP_LOGE("wasi_i2c", "Failed to get module instance.");
        return -1;
    }

    if (!wasm_runtime_validate_app_addr(instance, write_buff_ptr_idx, write_size))
    {
        ESP_LOGE("wasi_i2c", "Invalid write buffer address.");
        return -1;
    }

    if (!wasm_runtime_validate_app_addr(instance, read_buff_ptr_idx, read_size))
    {
        ESP_LOGE("wasi_i2c", "Invalid read buffer address.");
        return -1;
    }

    char *write_buff = (char *)wasm_runtime_addr_app_to_native(instance, write_buff_ptr_idx);
    char *read_buff  = (char *)wasm_runtime_addr_app_to_native(instance, read_buff_ptr_idx);

    esp_err_t err = i2c_master_write_read_device(
        (i2c_port_t)port, (uint8_t)addr,
        (const uint8_t *)write_buff, (size_t)write_size,
        (uint8_t *)read_buff,        (size_t)read_size,
        (TickType_t)ticks_to_wait);

    return (err == ESP_OK) ? 0 : -1;
}

int32_t gpio_set_pin_mode(wasm_exec_env_t exec_env, int32_t pin, int32_t dir)
{
    int32_t hw_pin = map_pin(pin);
    gpio_num_t p = (gpio_num_t)hw_pin;
    gpio_reset_pin(p);
    gpio_mode_t mode = (dir == 0) ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT;
    if (gpio_set_direction(p, mode) != ESP_OK)
    {
        return -1;
    }
    if (gpio_set_pull_mode(p, GPIO_FLOATING) != ESP_OK)
    {
        return -1;
    }
    return 0;
}

int32_t gpio_read(wasm_exec_env_t exec_env, int32_t pin)
{
    int32_t hw_pin = map_pin(pin);
    ESP_LOGI("WASI_GPIO", "gpio_read: virtual=%d mapped=%d", pin, hw_pin);
    return gpio_get_level(hw_pin);
}

int32_t gpio_write(wasm_exec_env_t exec_env, int32_t pin, int32_t value)
{
    int32_t hw_pin = map_pin(pin);
    ESP_LOGI("WASI_GPIO", "gpio_write: virtual=%d mapped=%d value=%d", pin, hw_pin, value);
    return (gpio_set_level(hw_pin, value ? 1 : 0) == ESP_OK) ? 0 : -1;
}

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

int32_t pwm_init(wasm_exec_env_t exec_env, int32_t pin, int32_t channel,
                 int32_t freq, int32_t resolution, int32_t speed_mode)
{
    int32_t hw_pin = map_pin(pin);
    ESP_LOGI("WASI_LEDC", "pwm_init: pin=%d mapped=%d, channel=%d, freq=%d, resolution=%d, speed_mode=%d",
             pin, hw_pin, channel, freq, resolution, speed_mode);

    ledc_timer_config_t tcfg = {
        .speed_mode = (ledc_mode_t)speed_mode,
        .duty_resolution = (ledc_timer_bit_t)resolution,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = freq,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&tcfg));

    ledc_channel_config_t ccfg = {
        .gpio_num = hw_pin,
        .speed_mode = (ledc_mode_t)speed_mode,
        .channel = channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ccfg));
    return 0;
}

int32_t pwm_set_duty(wasm_exec_env_t exec_env, int32_t channel, int32_t duty,
                     int32_t speed_mode)
{
    ESP_LOGI("WASI_LEDC", "pwm_set_duty: channel=%d, duty=%d, speed_mode=%d",
             channel, duty, speed_mode);
    ESP_ERROR_CHECK(ledc_set_duty((ledc_mode_t)speed_mode, channel, duty));
    return 0;
}

int32_t pwm_update_duty(wasm_exec_env_t exec_env, int32_t channel,
                        int32_t speed_mode)
{
    ESP_LOGI("WASI_LEDC", "pwm_update_duty: channel=%d, speed_mode=%d",
             channel, speed_mode);
    ESP_ERROR_CHECK(ledc_update_duty((ledc_mode_t)speed_mode, channel));
    return 0;
}

int32_t pwm_set_frequency(wasm_exec_env_t exec_env, int32_t channel,
                          int32_t freq, int32_t speed_mode)
{
    ESP_LOGI("WASI_LEDC", "pwm_set_frequency: channel=%d, freq=%d, speed_mode=%d",
             channel, freq, speed_mode);
    ESP_ERROR_CHECK(ledc_set_freq((ledc_mode_t)speed_mode, channel, freq));
    return 0;
}

#define WASI_MAX_SPI_HOSTS   3  /* SPI1_HOST, SPI2_HOST, SPI3_HOST */
#define WASI_MAX_SPI_DEVICES 4

typedef struct
{
    bool used;
    spi_device_handle_t handle;
} wasi_spi_device_t;

static bool s_spi_bus_initialized[WASI_MAX_SPI_HOSTS];
static wasi_spi_device_t s_spi_devices[WASI_MAX_SPI_DEVICES];

static bool is_valid_spi_host(int32_t host)
{
    return host >= 0 && host < WASI_MAX_SPI_HOSTS;
}

static int32_t alloc_spi_device(spi_device_handle_t handle)
{
    for (int i = 0; i < WASI_MAX_SPI_DEVICES; i++)
    {
        if (!s_spi_devices[i].used)
        {
            s_spi_devices[i].used = true;
            s_spi_devices[i].handle = handle;
            return (int32_t)i;
        }
    }
    return -1;
}

static spi_device_handle_t get_spi_device(int32_t h)
{
    if (h < 0 || h >= WASI_MAX_SPI_DEVICES || !s_spi_devices[h].used)
    {
        return NULL;
    }
    return s_spi_devices[h].handle;
}

int32_t waiot_spi_bus_init(wasm_exec_env_t exec_env, int32_t host,
                           int32_t mosi_gpio, int32_t miso_gpio,
                           int32_t sclk_gpio, int32_t max_transfer_sz)
{
    ESP_LOGI("wasi_spi", "spi_bus_init: host=%d, mosi=%d, miso=%d, sclk=%d, max_sz=%d",
             host, mosi_gpio, miso_gpio, sclk_gpio, max_transfer_sz);
    if (!is_valid_spi_host(host))
    {
        ESP_LOGE("wasi_spi", "spi_bus_init: invalid host %d", host);
        return -1;
    }
    if (s_spi_bus_initialized[host])
    {
        ESP_LOGI("wasi_spi", "spi_bus_init: host=%d already initialized, skipping", host);
        return 0;
    }
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = map_pin(mosi_gpio),
        .miso_io_num = map_pin(miso_gpio),
        .sclk_io_num = map_pin(sclk_gpio),
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = max_transfer_sz,
    };
    esp_err_t err = spi_bus_initialize((spi_host_device_t)host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK)
    {
        ESP_LOGE("wasi_spi", "spi_bus_initialize failed: %d", err);
        return -1;
    }
    s_spi_bus_initialized[host] = true;
    return 0;
}

int32_t waiot_spi_device_add(wasm_exec_env_t exec_env, int32_t host,
                             int32_t cs_gpio, int32_t mode, int32_t freq_hz)
{
    ESP_LOGI("wasi_spi", "spi_device_add: host=%d, cs=%d, mode=%d, freq=%d",
             host, cs_gpio, mode, freq_hz);
    if (!is_valid_spi_host(host))
    {
        ESP_LOGE("wasi_spi", "spi_device_add: invalid host %d", host);
        return -1;
    }
    if (!s_spi_bus_initialized[host])
    {
        ESP_LOGE("wasi_spi", "spi_device_add: host=%d is not initialized", host);
        return -1;
    }
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = freq_hz,
        .mode = (uint8_t)mode,
        .spics_io_num = map_pin(cs_gpio),
        .queue_size = 7,
    };
    spi_device_handle_t dev_handle;
    esp_err_t err = spi_bus_add_device((spi_host_device_t)host, &dev_cfg, &dev_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE("wasi_spi", "spi_bus_add_device failed: %d", err);
        return -1;
    }
    int32_t idx = alloc_spi_device(dev_handle);
    if (idx < 0)
    {
        ESP_LOGE("wasi_spi", "No free SPI device slots");
        spi_bus_remove_device(dev_handle);
        return -1;
    }
    ESP_LOGI("wasi_spi", "spi_device_add: assigned handle=%d", idx);
    return idx;
}

int32_t waiot_spi_acquire(wasm_exec_env_t exec_env, int32_t device_handle)
{
    spi_device_handle_t handle = get_spi_device(device_handle);
    if (!handle)
    {
        ESP_LOGE("wasi_spi", "spi_acquire: invalid handle %d", device_handle);
        return -1;
    }
    esp_err_t err = spi_device_acquire_bus(handle, portMAX_DELAY);
    return (err == ESP_OK) ? 0 : -1;
}

int32_t waiot_spi_release(wasm_exec_env_t exec_env, int32_t device_handle)
{
    spi_device_handle_t handle = get_spi_device(device_handle);
    if (!handle)
    {
        ESP_LOGE("wasi_spi", "spi_release: invalid handle %d", device_handle);
        return -1;
    }
    spi_device_release_bus(handle);
    return 0;
}

int32_t waiot_spi_transfer(wasm_exec_env_t exec_env, int32_t device_handle,
                           int32_t tx_ptr, int32_t rx_ptr, int32_t len)
{
    if (len < 0)
    {
        ESP_LOGE("wasi_spi", "spi_transfer: invalid length %d", len);
        return -1;
    }
    if (len == 0)
    {
        return 0;
    }
    if (len > (INT32_MAX / 8))
    {
        ESP_LOGE("wasi_spi", "spi_transfer: len too large %d", len);
        return -1;
    }
    if (tx_ptr == 0 && rx_ptr == 0)
    {
        ESP_LOGE("wasi_spi", "spi_transfer: both tx_ptr and rx_ptr are 0");
        return -1;
    }

    spi_device_handle_t handle = get_spi_device(device_handle);
    if (!handle)
    {
        ESP_LOGE("wasi_spi", "spi_transfer: invalid handle %d", device_handle);
        return -1;
    }

    wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
    if (!instance)
    {
        ESP_LOGE("wasi_spi", "Failed to get module instance.");
        return -1;
    }

    void *tx_buf = NULL;
    void *rx_buf = NULL;

    if (tx_ptr != 0)
    {
        if (!wasm_runtime_validate_app_addr(instance, tx_ptr, (uint32_t)len))
        {
            ESP_LOGE("wasi_spi", "Invalid tx buffer address.");
            return -1;
        }
        tx_buf = wasm_runtime_addr_app_to_native(instance, tx_ptr);
    }
    if (rx_ptr != 0)
    {
        if (!wasm_runtime_validate_app_addr(instance, rx_ptr, (uint32_t)len))
        {
            ESP_LOGE("wasi_spi", "Invalid rx buffer address.");
            return -1;
        }
        rx_buf = wasm_runtime_addr_app_to_native(instance, rx_ptr);
    }

    spi_transaction_t t = {
        .length = tx_buf ? (size_t)len * 8 : 0,
        .rxlength = rx_buf ? (size_t)len * 8 : 0,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
    };
    esp_err_t err = spi_device_polling_transmit(handle, &t);
    return (err == ESP_OK) ? 0 : -1;
}

int32_t fd_write(wasm_exec_env_t exec_env, int32_t fd, int32_t buf_iovec_addr,
                 int32_t vec_len, int32_t size_addr)
{
    ESP_LOGI("wasi_fd", "fd_write: fd=%d, buf_iovec_addr=0x%08x, vec_len=%d, size_addr=0x%08x",
             fd, buf_iovec_addr, vec_len, size_addr);
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
    wasi_iovec_t *iovecs = (wasi_iovec_t *)wasm_runtime_addr_app_to_native(inst, buf_iovec_addr);
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
        const uint8_t *ptr = (const uint8_t *)wasm_runtime_addr_app_to_native(inst, base);
        if (!ptr)
        {
            return -8;
        }
        if (log_append(ptr, (uint32_t)len) != 0)
        {
            return -9;
        }
        total_written += (uint32_t)len;
    }

    if (!wasm_runtime_validate_app_addr(inst, size_addr, sizeof(uint32_t)))
    {
        return -10;
    }
    uint32_t *nwritten_ptr = (uint32_t *)wasm_runtime_addr_app_to_native(inst, size_addr);
    if (!nwritten_ptr)
    {
        return -11;
    }
    *nwritten_ptr = total_written;

    return 0;
}

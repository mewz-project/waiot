#include "wasi.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_log.h"

typedef int32_t wasi_bus_handle_t;
typedef int32_t wasi_gpio_handle_t;

#define WASI_MAX_I2C_BUSES 2
#define WASI_MAX_GPIO_PINS 16

typedef struct
{
    bool used;
    i2c_port_t port;
    gpio_num_t sda;
    gpio_num_t scl;
    uint32_t freq_hz;
} wasi_i2c_bus_t;

typedef struct
{
    bool used;
    gpio_num_t pin;
} wasi_gpio_t;

static wasi_i2c_bus_t s_i2c[WASI_MAX_I2C_BUSES];
static wasi_gpio_t s_gpio[WASI_MAX_GPIO_PINS];

static inline wasi_i2c_bus_t *
get_bus(wasi_bus_handle_t h)
{
    if (h < 0 || h >= WASI_MAX_I2C_BUSES)
    {
        return NULL;
    }
    return s_i2c[h].used ? &s_i2c[h] : NULL;
}
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
int32_t
i2c_open_bus(wasm_exec_env_t exec_env, int32_t port, int32_t sda_gpio,
             int32_t scl_gpio, int32_t freq_hz)
{
    int slot = -1;
    for (int i = 0; i < WASI_MAX_I2C_BUSES; i++)
    {
        if (!s_i2c[i].used)
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        return -1;
    }

    i2c_config_t conf = {.mode = I2C_MODE_MASTER,
                         .sda_io_num = (gpio_num_t)sda_gpio,
                         .scl_io_num = (gpio_num_t)scl_gpio,
                         .sda_pullup_en = GPIO_PULLUP_ENABLE,
                         .scl_pullup_en = GPIO_PULLUP_ENABLE,
                         .master.clk_speed = (uint32_t)freq_hz};
    esp_err_t err;
    err = i2c_param_config((i2c_port_t)port, &conf);
    if (err != ESP_OK)
    {
        return -2;
    }

    err = i2c_driver_install((i2c_port_t)port, conf.mode, 0, 0, 0);
    if (err != ESP_OK)
    {
        return -3;
    }

    s_i2c[slot].used = true;
    s_i2c[slot].port = (i2c_port_t)port;
    s_i2c[slot].sda = (gpio_num_t)sda_gpio;
    s_i2c[slot].scl = (gpio_num_t)scl_gpio;
    s_i2c[slot].freq_hz = (uint32_t)freq_hz;
    return slot; // = handle
}

int32_t
i2c_write(wasm_exec_env_t exec_env, int32_t bus_handle, int32_t addr7,
          const uint8_t *data, uint32_t len, int32_t stop)
{
    // TODO: stop is currently unused since ESP-IDF's i2c_master_write_to_device
    // API does not support it.
    (void)stop;
    wasi_i2c_bus_t *b = get_bus(bus_handle);
    if (!b)
    {
        return -1;
    }

    esp_err_t err = i2c_master_write_to_device(b->port, (uint8_t)addr7, data,
                                               len, 1000 / portTICK_PERIOD_MS);
    return (err == ESP_OK) ? 0 : -2;
}

int32_t
i2c_read(wasm_exec_env_t exec_env, int32_t bus_handle, int32_t addr7,
         uint8_t *out, uint32_t len, int32_t stop)
{
    (void)stop;
    wasi_i2c_bus_t *b = get_bus(bus_handle);
    if (!b)
    {
        return -1;
    }
    esp_err_t err = i2c_master_read_from_device(b->port, (uint8_t)addr7, out,
                                                len, 1000 / portTICK_PERIOD_MS);
    return (err == ESP_OK) ? 0 : -2;
}

int32_t
i2c_write_read(wasm_exec_env_t exec_env, int32_t bus_handle, int32_t addr7,
               const uint8_t *tx, uint32_t tx_len, uint8_t *rx, uint32_t rx_len)
{
    wasi_i2c_bus_t *b = get_bus(bus_handle);
    if (!b)
    {
        return -1;
    }

    esp_err_t err =
        i2c_master_write_read_device(b->port, (uint8_t)addr7, tx, tx_len, rx,
                                     rx_len, 1000 / portTICK_PERIOD_MS);
    return (err == ESP_OK) ? 0 : -2;
}

//====================== GPIO ======================

int32_t
gpio_set_pin_mode(wasm_exec_env_t exec_env, int32_t pin, int32_t dir)
{
    gpio_num_t p = (gpio_num_t)pin;
    gpio_reset_pin(p);
    gpio_mode_t mode = (dir == 0) ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT;
    gpio_pull_mode_t pm = GPIO_FLOATING;
    // if (pull == 1)
    // {
    //     pm = GPIO_PULLUP_ONLY;
    // }
    // else if (pull == 2)
    // {
    //     pm = GPIO_PULLDOWN_ONLY;
    // }
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
    ESP_LOGI("WASI_GPIO", "gpio_read: pin=%d", pin);
    return gpio_get_level(pin);
}

int32_t
gpio_write(wasm_exec_env_t exec_env, int32_t pin, int32_t value)
{
    ESP_LOGI("WASI_GPIO", "gpio_write: pin=%d value=%d", pin, value);
    return (gpio_set_level(pin, value ? 1 : 0) == ESP_OK) ? 0 : -1;
}

// ====================== Timer ======================

int64_t
mono_now_ns(wasm_exec_env_t exec_env)
{
    // esp_timer_get_time(): microseconds since startup (monotonic)
    int64_t us = (int64_t)esp_timer_get_time();
    return us * 1000; // to nanoseconds
}

int32_t
mono_sleep_ns(wasm_exec_env_t exec_env, int64_t ns)
{
    if (ns <= 0)
        return 0;
    uint64_t ms = (uint64_t)(ns / 1000000);
    if (ms == 0)
        ms = 1;
    vTaskDelay(pdMS_TO_TICKS(ms));
    return 0;
}

// ====================== LEDC ======================

int32_t waiot_ledc_init(wasm_exec_env_t exec_env, uint32_t pin, uint32_t channel, uint32_t freq, uint32_t resolution)
{
    // --- Configure LEDC timer ---
    ledc_timer_config_t tcfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = resolution,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = freq,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&tcfg));

    // --- Configure LEDC channel ---
    ledc_channel_config_t ccfg = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
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

int32_t waiot_ledc_set_duty(wasm_exec_env_t exec_env, uint32_t channel, uint32_t duty)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty));
    return 0;
}

int32_t waiot_ledc_set_freq(wasm_exec_env_t exec_env, uint32_t channel, uint32_t freq)
{
    ESP_ERROR_CHECK(ledc_set_freq(LEDC_LOW_SPEED_MODE, channel, freq));
    return 0;
}

int32_t waiot_ledc_update_duty(wasm_exec_env_t exec_env, uint32_t channel)
{
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, channel));
    return 0;
}
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define IWASM_MAIN_STACK_SIZE 8192
#define REGISTER_MAX_UPLOAD_SIZE (512 * 1024)
#define LOG_TAG "waiot"

#define CONFIG_NAMESPACE "config"

typedef struct {
    char sta_ssid[32];
    char sta_pass[64];
    char ap_ssid[32];
    char ap_pass[64];
    bool sta_mode;
} wifi_settings_t;

typedef struct {
    char device_name[64];
    char kubelet_endpoint[64];
    char cpu_info[64];
    char memory_info[64];
} device_settings_t;

esp_err_t load_wifi_config_from_nvs(wifi_settings_t *config);
esp_err_t load_device_settings_from_nvs(device_settings_t *settings);

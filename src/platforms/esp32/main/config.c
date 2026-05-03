/*
 * Copyright (C) 2019-21 Intel Corporation and others.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "config.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include <string.h>

esp_err_t load_wifi_config_from_nvs(wifi_settings_t *config)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t required_size;

    ESP_LOGI(LOG_TAG, "Opening NVS namespace '%s' from partition '%s'...", CONFIG_NAMESPACE, CONFIG_NAMESPACE);
    err = nvs_open_from_partition(CONFIG_NAMESPACE, CONFIG_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Error opening NVS handle from config partition: %s", esp_err_to_name(err));
        ESP_LOGE(LOG_TAG, "Partition: config, Namespace: %s", CONFIG_NAMESPACE);
        return err;
    }
    ESP_LOGI(LOG_TAG, "NVS handle opened successfully");

    required_size = sizeof(config->sta_ssid);
    err = nvs_get_str(nvs_handle, "sta_ssid", config->sta_ssid, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "STA SSID not found in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    required_size = sizeof(config->sta_pass);
    err = nvs_get_str(nvs_handle, "sta_pass", config->sta_pass, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "STA password not found in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    required_size = sizeof(config->ap_ssid);
    err = nvs_get_str(nvs_handle, "ap_ssid", config->ap_ssid, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "AP SSID not found in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    required_size = sizeof(config->ap_pass);
    err = nvs_get_str(nvs_handle, "ap_pass", config->ap_pass, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "AP password not found in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    uint8_t sta_mode_u8;
    err = nvs_get_u8(nvs_handle, "sta_mode", &sta_mode_u8);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "STA mode not found in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    config->sta_mode = (bool)sta_mode_u8;

    nvs_close(nvs_handle);
    ESP_LOGI(LOG_TAG, "WiFi configuration loaded from NVS");
    return ESP_OK;
}

esp_err_t load_device_settings_from_nvs(device_settings_t *settings)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t required_size;

    ESP_LOGI(LOG_TAG, "Opening NVS namespace '%s' from partition '%s' for device settings...", CONFIG_NAMESPACE, CONFIG_NAMESPACE);
    err = nvs_open_from_partition(CONFIG_NAMESPACE, CONFIG_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Error opening NVS handle from config partition: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(LOG_TAG, "NVS handle opened successfully for device settings");

    required_size = sizeof(settings->device_name);
    err = nvs_get_str(nvs_handle, "device_name", settings->device_name, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Device name not found in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    required_size = sizeof(settings->kubelet_endpoint);
    err = nvs_get_str(nvs_handle, "kubelet_ep", settings->kubelet_endpoint, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Kubelet endpoint not found in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    required_size = sizeof(settings->cpu_info);
    err = nvs_get_str(nvs_handle, "cpu_info", settings->cpu_info, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "CPU info not found in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    required_size = sizeof(settings->memory_info);
    err = nvs_get_str(nvs_handle, "memory_info", settings->memory_info, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Memory info not found in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(LOG_TAG, "Device settings loaded from NVS");
    return ESP_OK;
}

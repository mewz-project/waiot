/*
 * Copyright (C) 2019-21 Intel Corporation and others.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "config.h"
#include "esp_http.h"
#include "platform_wamr.h"
#include "wamr.h"

#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static httpd_handle_t s_server = NULL;
static device_settings_t s_device_settings = {0};

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(LOG_TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(LOG_TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(LOG_TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGI(LOG_TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(LOG_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            printf("%.*s", evt->data_len, (char *)evt->data);
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(LOG_TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(LOG_TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGI(LOG_TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

static esp_err_t register_device_to_kubelet(const char *kubelet_endpoint,
                                            const char *device_name,
                                            const char *device_ip,
                                            const char *cpu_info,
                                            const char *memory_info)
{
    ESP_LOGI(LOG_TAG, "Registering device to Kubelet at %s", kubelet_endpoint);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        ESP_LOGE(LOG_TAG, "Failed to create JSON object");
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(root, "name", device_name);
    cJSON_AddStringToObject(root, "ip", device_ip);
    cJSON *annotations = cJSON_AddObjectToObject(root, "annotations");
    cJSON_AddStringToObject(annotations, "gpio.virtual-kubelet.io/buzzer", "25");
    cJSON_AddStringToObject(root, "cpu", cpu_info);
    cJSON_AddStringToObject(root, "memory", memory_info);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL)
    {
        ESP_LOGE(LOG_TAG, "Failed to print JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    ESP_LOGI(LOG_TAG, "JSON payload: %s", json_str);

    char url[128];
    snprintf(url, sizeof(url), "http://%s/register", kubelet_endpoint);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(LOG_TAG, "Failed to initialize HTTP client");
        free(json_str);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(LOG_TAG, "Kubelet registration response status = %d", status_code);
        if (status_code == 200)
        {
            ESP_LOGI(LOG_TAG, "Device successfully registered to Kubelet");
        }
        else
        {
            ESP_LOGW(LOG_TAG, "Kubelet registration returned status code %d", status_code);
            err = ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGE(LOG_TAG, "Kubelet registration failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(json_str);
    cJSON_Delete(root);

    return err;
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(LOG_TAG, "Wi-Fi disconnected, stopping server and reconnecting...");
        waiot_stop_http_server(s_server);
        s_server = NULL;
        esp_wifi_connect();
    }
}

static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(LOG_TAG, "Got STA IP: " IPSTR, IP2STR(&e->ip_info.ip));

        if (strlen(s_device_settings.kubelet_endpoint) > 0)
        {
            char ip_str[16];
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&e->ip_info.ip));

            esp_err_t err = register_device_to_kubelet(
                s_device_settings.kubelet_endpoint,
                s_device_settings.device_name,
                ip_str,
                s_device_settings.cpu_info,
                s_device_settings.memory_info);

            if (err != ESP_OK)
            {
                ESP_LOGW(LOG_TAG, "Failed to register device to Kubelet, but continuing...");
            }
        }
        else
        {
            ESP_LOGW(LOG_TAG, "Device settings not loaded, skipping Kubelet registration");
        }

        if (!s_server)
        {
            s_server = waiot_start_http_server();
        }
    }
}

static void init_wifi_sta(const char *ssid, const char *pass)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL, NULL));

    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, pass, sizeof(wc.sta.password) - 1);
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_LOGI(LOG_TAG, "Connecting to SSID:%s...", ssid);
}

static void init_wifi_ap(const char *ssid, const char *pass)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL, NULL));

    wifi_config_t wc = {0};
    strncpy((char *)wc.ap.ssid, ssid, sizeof(wc.ap.ssid) - 1);
    wc.ap.ssid_len = strlen(ssid);
    strncpy((char *)wc.ap.password, pass, sizeof(wc.ap.password) - 1);

    wc.ap.channel = 1;
    wc.ap.max_connection = 2;
    wc.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    if (strlen(pass) == 0)
    {
        wc.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(LOG_TAG, "WiFi AP started. SSID:%s password:%s channel:%d",
             ssid, pass, wc.ap.channel);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nvs_flash_init_partition(CONFIG_NAMESPACE);
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(LOG_TAG, "NVS partition needs to be erased, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase_partition(CONFIG_NAMESPACE));
        ret = nvs_flash_init_partition(CONFIG_NAMESPACE);
        ESP_LOGI(LOG_TAG, "After erase, nvs_flash_init_partition returned: %s", esp_err_to_name(ret));
    }
    ESP_ERROR_CHECK(ret);

    wifi_settings_t wifi_config;
    ESP_ERROR_CHECK(load_wifi_config_from_nvs(&wifi_config));

    esp_err_t dev_err = load_device_settings_from_nvs(&s_device_settings);
    if (dev_err != ESP_OK)
    {
        ESP_LOGW(LOG_TAG, "Device settings not found in NVS, Kubelet registration will be skipped");
        memset(&s_device_settings, 0, sizeof(s_device_settings));
    }
    else
    {
        ESP_LOGI(LOG_TAG, "Device settings loaded: name=%s, kubelet_endpoint=%s, cpu=%s, memory=%s",
                 s_device_settings.device_name, s_device_settings.kubelet_endpoint,
                 s_device_settings.cpu_info, s_device_settings.memory_info);
    }

    if (wifi_config.sta_mode)
    {
        init_wifi_sta(wifi_config.sta_ssid, wifi_config.sta_pass);
    }
    else
    {
        init_wifi_ap(wifi_config.ap_ssid, wifi_config.ap_pass);
    }

    waiot_platform_configure_wamr();
    init_wamr();

    if (!wifi_config.sta_mode)
    {
        s_server = waiot_start_http_server();
    }

    ESP_LOGI(LOG_TAG, "Exiting...");
}

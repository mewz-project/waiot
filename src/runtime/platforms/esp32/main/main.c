/*
 * Copyright (C) 2019-21 Intel Corporation and others.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wasm_export.h"
#include "bh_platform.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "cJSON.h"

#include "config.h"
#include "wamr.h"
#include "wasi.h"
#include "pinmap.h"
#include "cJSON.h"

static httpd_handle_t s_server = NULL;
static device_settings_t s_device_settings = {0};

void print_mem_info(void)
{
    printf("Free heap: %u\n", (unsigned)esp_get_free_heap_size());
    printf("Free internal DRAM: %u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

/* ===== HTTP Handlers ===== */
// POST /gpio/mapping
static esp_err_t
pinmap_handler(httpd_req_t *req)
{
    ESP_LOGI(LOG_TAG, "Received request /gpio/mapping");

    size_t total = req->content_len;
    if (total <= 0 || total > 40960)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body size");
    }
    char *buf = (char *)malloc(total + 1);
    if (!buf)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    }
    size_t received = 0;
    while (received < total)
    {
        int32_t r = httpd_req_recv(req, buf + received, total - received);
        if (r <= 0)
        {
            if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
            free(buf);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
        }
        received += r;
    }
    buf[received] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root)
    {
        free(buf);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid JSON");
    }

    if (!cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        free(buf);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "expected top-level object { \"virtual\": physical, ... }");
    }

    pinmap_reset_identity();

    for (cJSON *it = root->child; it != NULL; it = it->next)
    {
        if (!it->string || !cJSON_IsNumber(it))
        {
            cJSON_Delete(root);
            free(buf);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid mapping entry");
        }

        char *endptr = NULL;
        long vpin_long = strtol(it->string, &endptr, 10);
        if (endptr == it->string || *endptr != '\0')
        {
            cJSON_Delete(root);
            free(buf);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "object keys must be integer strings");
        }
        if (vpin_long < 0 || vpin_long >= MAX_GPIO)
        {
            cJSON_Delete(root);
            free(buf);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "virtual pin out of range");
        }

        int32_t virtual_pin = (int32_t)vpin_long;
        int32_t physical_pin = (int32_t)it->valuedouble;
        int rc = pinmap_set_virtual_to_physical(virtual_pin, physical_pin);
        if (rc != 0)
        {
            cJSON_Delete(root);
            free(buf);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "physical pin out of range");
        }
    }

    cJSON_Delete(root);
    free(buf);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

// GET /
static esp_err_t
root_get_handler(httpd_req_t *req)
{
    ESP_LOGI(LOG_TAG, "Received request /");
    const char *resp = "Hello M5Stick C-Plus\n";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// GET /logs
static esp_err_t
logs_get_handler(httpd_req_t *req)
{
    ESP_LOGI(LOG_TAG, "Received request /logs");

    // Optional query: ?tail=4096
    int tail = 0;
    char buf[32];
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1 && buf_len < sizeof(buf))
    {
        char *q = (char *)malloc(buf_len);
        if (q)
        {
            if (httpd_req_get_url_query_str(req, q, buf_len) == ESP_OK)
            {
                if (httpd_query_key_value(q, "tail", buf, sizeof(buf)) == ESP_OK)
                {
                    tail = atoi(buf);
                    if (tail < 0)
                        tail = 0;
                }
            }
            free(q);
        }
    }

    int filled = log_get_filled();
    if (filled < 0)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "log error");
    }

    int to_send = filled;
    if (tail > 0 && tail < filled)
    {
        to_send = tail;
    }

    // Build response
    uint8_t *out = (uint8_t *)malloc((size_t)to_send + 1);
    if (!out)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    }
    int offset = filled - to_send;
    int n = log_read_from_oldest((uint32_t)offset, out, (uint32_t)to_send);
    if (n < 0)
    {
        free(out);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "read error");
    }
    out[n] = '\0';

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, (const char *)out, n);
    free(out);
    return ESP_OK;
}

// POST /create
static esp_err_t
create_handler(httpd_req_t *req)
{
    ESP_LOGI(LOG_TAG, "Received request /create");

    // Reject if a Wasm instance is already running
    if (is_wasm_instance_running())
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Wasm instance already running. Stop it first.\n");
        return ESP_OK;
    }

    // Read Wasm binary
    esp_err_t err = receive_wasm_binary(req);
    if (err != ESP_OK)
    {
        const char *msg = "Failed to receive Wasm binary\n";
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, msg);
        return ESP_OK;
    }

    // Launch new Wasm instance
    launch_new_wasm_instance();

    const char *resp = "Wasm instance created\n";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// POST /stop
static esp_err_t
stop_handler(httpd_req_t *req)
{
    ESP_LOGI(LOG_TAG, "Received request /stop");

    stop_current_wasm_instance();

    const char *resp = "Wasm instance stopped\n";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ===== HTTP Server ===== */

static httpd_handle_t
start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t root = {.uri = "/",
                            .method = HTTP_GET,
                            .handler = root_get_handler};
        httpd_uri_t create = {.uri = "/create",
                              .method = HTTP_POST,
                              .handler = create_handler};
        httpd_uri_t stop = {.uri = "/stop",
                            .method = HTTP_POST,
                            .handler = stop_handler};
        httpd_uri_t logs = {.uri = "/logs",
                            .method = HTTP_GET,
                            .handler = logs_get_handler};
        httpd_uri_t pinmap_apply = {.uri = "/gpio/mapping",
                                    .method = HTTP_POST,
                                    .handler = pinmap_handler};

        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &create);
        httpd_register_uri_handler(server, &stop);
        httpd_register_uri_handler(server, &pinmap_apply);
        httpd_register_uri_handler(server, &logs);
        ESP_LOGI(LOG_TAG, "HTTP server started on port %d", config.server_port);
    }
    return server;
}

static void
stop_webserver(httpd_handle_t server)
{
    if (server)
    {
        httpd_stop(server);
        ESP_LOGI(LOG_TAG, "HTTP server stopped");
    }
}

/* ===== Device Registration to Kubelet ===== */

// HTTP event handler for kubelet registration request
static esp_err_t
http_event_handler(esp_http_client_event_t *evt)
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
            // Response data might contain useful information
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

// Register device to Kubelet
static esp_err_t
register_device_to_kubelet(const char *kubelet_endpoint, const char *device_name, 
                          const char *device_ip, const char *cpu_info, 
                          const char *memory_info)
{
    ESP_LOGI(LOG_TAG, "Registering device to Kubelet at %s", kubelet_endpoint);

    // Create JSON payload
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        ESP_LOGE(LOG_TAG, "Failed to create JSON object");
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(root, "name", device_name);
    cJSON_AddStringToObject(root, "ip", device_ip);
    cJSON *annotations = cJSON_AddObjectToObject(root, "annotations");  // Empty annotations object
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

    // Prepare URL
    char url[128];
    snprintf(url, sizeof(url), "http://%s/register", kubelet_endpoint);

    // Configure HTTP client
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

    // Set headers and payload
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    // Perform the request
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

    // Cleanup
    esp_http_client_cleanup(client);
    free(json_str);
    cJSON_Delete(root);

    return err;
}

/* ===== Wi-Fi Event Handlers ===== */

static void
on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(LOG_TAG, "Wi-Fi disconnected, stopping server and reconnecting…");
        stop_webserver(s_server);
        s_server = NULL;
        esp_wifi_connect();
    }
}

static void
on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(LOG_TAG, "Got STA IP: " IPSTR, IP2STR(&e->ip_info.ip));
        
        // Register device to Kubelet if device settings are loaded
        if (strlen(s_device_settings.kubelet_endpoint) > 0)
        {
            // Convert IP address to string
            char ip_str[16];
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&e->ip_info.ip));
            
            esp_err_t err = register_device_to_kubelet(
                s_device_settings.kubelet_endpoint,
                s_device_settings.device_name,
                ip_str,
                s_device_settings.cpu_info,
                s_device_settings.memory_info
            );
            
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
            s_server = start_http_server();
        }
    }
}

/* ========= Wi-Fi ======== */
void init_wifi_sta(const char *ssid, const char *pass)
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
    ESP_LOGI(LOG_TAG, "Connecting to SSID:%s…", ssid);
}

void init_wifi_ap(const char *ssid, const char *pass)
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
    // Initialize NVS (required for Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
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

    // Load device settings from NVS (optional, for Kubelet registration)
    esp_err_t dev_err = load_device_settings_from_nvs(&s_device_settings);
    if (dev_err != ESP_OK)
    {
        ESP_LOGW(LOG_TAG, "Device settings not found in NVS, Kubelet registration will be skipped");
        // Clear the device settings to indicate they are not loaded
        memset(&s_device_settings, 0, sizeof(s_device_settings));
    }
    else
    {
        ESP_LOGI(LOG_TAG, "Device settings loaded: name=%s, kubelet_endpoint=%s, cpu=%s, memory=%s",
                 s_device_settings.device_name, s_device_settings.kubelet_endpoint,
                 s_device_settings.cpu_info, s_device_settings.memory_info);
    }

    // Initialize Wi-Fi
    if (wifi_config.sta_mode)
    {
        init_wifi_sta(wifi_config.sta_ssid, wifi_config.sta_pass);
    }
    else
    {
        init_wifi_ap(wifi_config.ap_ssid, wifi_config.ap_pass);
    }

    // Initialize WAMR
    init_wamr();
    init_http_client();

    // Launch HTTP Server
    s_server = start_http_server();

    ESP_LOGI(LOG_TAG, "Exiting...");
}

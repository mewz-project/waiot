/*
 * Copyright (C) 2019-21 Intel Corporation and others.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include <stdio.h>
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
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"

#include "config.h"
#include "wasi_m5.h"
#include "wamr.h"
#include "wasi.h"
#include "utils.h"

// WiFi mode
// sta_mode == true: connect to WiFi
// sta_mode == false: serve as an AP
bool sta_mode = true;

static httpd_handle_t s_server = NULL;

/* ===== HTTP Handlers ===== */

// GET /
static esp_err_t
root_get_handler(httpd_req_t *req)
{
    ESP_LOGI(LOG_TAG, "Received request /");

    // Respond with device information
    esp_chip_info_t info;
    esp_chip_info(&info);
    const char *model = convert_chip_model_to_str(info.model);
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char resp[256];
    snprintf(resp, sizeof(resp),
             "Model: %s\n"
             "MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n"
             "Free internal RAM: %u\n",
             model,
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
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

        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &create);
        httpd_register_uri_handler(server, &stop);
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
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize Wi-Fi
    if (sta_mode)
    {
        init_wifi_sta(WIFI_STA_SSID, WIFI_STA_PASS);
    }
    else
    {
        init_wifi_ap(WIFI_AP_SSID, WIFI_AP_PASS);
    }

    // Initialize WAMR
    init_wamr();

    // Launch HTTP Server
    s_server = start_http_server();

    ESP_LOGI(LOG_TAG, "Exiting...");
}

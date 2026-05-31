#include "esp_http.h"

#include "config.h"
#include "http.h"

#include "esp_log.h"

#include <stdlib.h>
#include <string.h>

static esp_err_t send_core_response(httpd_req_t *req,
                                    waiot_http_text_response_t response)
{
    if (response.status != WAIOT_HTTP_OK)
    {
        return httpd_resp_send_err(req, (httpd_err_code_t)response.status,
                                   response.body);
    }

    httpd_resp_set_type(req, response.content_type);
    httpd_resp_send(req, response.body, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t read_request_body(httpd_req_t *req, size_t max_size,
                                   uint8_t **out, size_t *out_len)
{
    size_t total = req->content_len;
    if (total == 0 || total > max_size)
    {
        return ESP_FAIL;
    }

    uint8_t *buf = (uint8_t *)malloc(total + 1);
    if (!buf)
    {
        return ESP_ERR_NO_MEM;
    }

    size_t received = 0;
    while (received < total)
    {
        int32_t r = httpd_req_recv(req, (char *)buf + received, total - received);
        if (r <= 0)
        {
            if (r == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            free(buf);
            return ESP_FAIL;
        }
        received += (size_t)r;
    }
    buf[received] = '\0';

    *out = buf;
    *out_len = received;
    return ESP_OK;
}

static esp_err_t pinmap_handler(httpd_req_t *req)
{
    ESP_LOGI(LOG_TAG, "Received request /gpio/mapping");

    uint8_t *body = NULL;
    size_t body_len = 0;
    esp_err_t err = read_request_body(req, 40960, &body, &body_len);
    if (err == ESP_ERR_NO_MEM)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    }
    if (err != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body size");
    }

    waiot_http_text_response_t response =
        waiot_http_apply_pinmap_json((const char *)body, body_len);
    free(body);
    return send_core_response(req, response);
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    ESP_LOGI(LOG_TAG, "Received request /");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, waiot_http_root_body(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t logs_get_handler(httpd_req_t *req)
{
    ESP_LOGI(LOG_TAG, "Received request /logs");

    int tail = 0;
    char buf[32];
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1 && buf_len < sizeof(buf))
    {
        char *q = (char *)malloc(buf_len);
        if (q)
        {
            if (httpd_req_get_url_query_str(req, q, buf_len) == ESP_OK &&
                httpd_query_key_value(q, "tail", buf, sizeof(buf)) == ESP_OK)
            {
                tail = atoi(buf);
            }
            free(q);
        }
    }

    uint8_t *out = NULL;
    size_t out_len = 0;
    const char *error_message = NULL;
    waiot_http_status_t status =
        waiot_http_get_logs(tail, &out, &out_len, &error_message);
    if (status != WAIOT_HTTP_OK)
    {
        return httpd_resp_send_err(req, (httpd_err_code_t)status, error_message);
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, (const char *)out, out_len);
    free(out);
    return ESP_OK;
}

static esp_err_t create_handler(httpd_req_t *req)
{
    ESP_LOGI(LOG_TAG, "Received request /create");

    uint8_t *body = NULL;
    size_t body_len = 0;
    esp_err_t err = read_request_body(req, REGISTER_MAX_UPLOAD_SIZE, &body, &body_len);
    if (err == ESP_ERR_NO_MEM)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    }
    if (err != ESP_OK)
    {
        if (req->content_len > REGISTER_MAX_UPLOAD_SIZE)
        {
            return httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE,
                                       "File too large");
        }
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
    }

    waiot_http_text_response_t response = waiot_http_create(body, body_len);
    free(body);
    return send_core_response(req, response);
}

static esp_err_t stop_handler(httpd_req_t *req)
{
    ESP_LOGI(LOG_TAG, "Received request /stop");
    return send_core_response(req, waiot_http_stop());
}

httpd_handle_t waiot_start_http_server(void)
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

void waiot_stop_http_server(httpd_handle_t server)
{
    if (server)
    {
        httpd_stop(server);
        ESP_LOGI(LOG_TAG, "HTTP server stopped");
    }
}

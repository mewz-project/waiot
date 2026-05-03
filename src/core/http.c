#include "http.h"

#include "log.h"
#include "pinmap.h"
#include "wamr.h"

#include "cJSON.h"

#include <stdlib.h>
#include <string.h>

const char *waiot_http_root_body(void)
{
    return "Hello M5Stick C-Plus\n";
}

waiot_http_text_response_t waiot_http_create(const uint8_t *wasm_data,
                                             size_t wasm_size)
{
    if (is_wasm_instance_running())
    {
        return (waiot_http_text_response_t){
            .status = WAIOT_HTTP_BAD_REQUEST,
            .content_type = "text/plain",
            .body = "Wasm instance already running. Stop it first.\n",
        };
    }

    if (wamr_set_wasm_binary(wasm_data, wasm_size) != 0)
    {
        return (waiot_http_text_response_t){
            .status = WAIOT_HTTP_INTERNAL_ERROR,
            .content_type = "text/plain",
            .body = "Failed to receive Wasm binary\n",
        };
    }

    launch_new_wasm_instance();
    return (waiot_http_text_response_t){
        .status = WAIOT_HTTP_OK,
        .content_type = "text/html",
        .body = "Wasm instance created\n",
    };
}

waiot_http_text_response_t waiot_http_stop(void)
{
    stop_current_wasm_instance();
    return (waiot_http_text_response_t){
        .status = WAIOT_HTTP_OK,
        .content_type = "text/html",
        .body = "Wasm instance stopped\n",
    };
}

waiot_http_text_response_t waiot_http_apply_pinmap_json(const char *body,
                                                        size_t len)
{
    (void)len;
    cJSON *root = cJSON_Parse(body);
    if (!root)
    {
        return (waiot_http_text_response_t){
            .status = WAIOT_HTTP_BAD_REQUEST,
            .content_type = "text/plain",
            .body = "invalid JSON",
        };
    }

    if (!cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        return (waiot_http_text_response_t){
            .status = WAIOT_HTTP_BAD_REQUEST,
            .content_type = "text/plain",
            .body = "expected top-level object { \"virtual\": physical, ... }",
        };
    }

    pinmap_reset_identity();

    for (cJSON *it = root->child; it != NULL; it = it->next)
    {
        if (!it->string || !cJSON_IsNumber(it))
        {
            cJSON_Delete(root);
            return (waiot_http_text_response_t){
                .status = WAIOT_HTTP_BAD_REQUEST,
                .content_type = "text/plain",
                .body = "invalid mapping entry",
            };
        }

        char *endptr = NULL;
        long vpin_long = strtol(it->string, &endptr, 10);
        if (endptr == it->string || *endptr != '\0')
        {
            cJSON_Delete(root);
            return (waiot_http_text_response_t){
                .status = WAIOT_HTTP_BAD_REQUEST,
                .content_type = "text/plain",
                .body = "object keys must be integer strings",
            };
        }
        if (vpin_long < 0 || vpin_long >= MAX_GPIO)
        {
            cJSON_Delete(root);
            return (waiot_http_text_response_t){
                .status = WAIOT_HTTP_BAD_REQUEST,
                .content_type = "text/plain",
                .body = "virtual pin out of range",
            };
        }

        int32_t physical_pin = (int32_t)it->valuedouble;
        if (pinmap_set_virtual_to_physical((int32_t)vpin_long, physical_pin) != 0)
        {
            cJSON_Delete(root);
            return (waiot_http_text_response_t){
                .status = WAIOT_HTTP_BAD_REQUEST,
                .content_type = "text/plain",
                .body = "physical pin out of range",
            };
        }
    }

    cJSON_Delete(root);
    return (waiot_http_text_response_t){
        .status = WAIOT_HTTP_OK,
        .content_type = "application/json",
        .body = "{\"status\":\"ok\"}",
    };
}

waiot_http_status_t waiot_http_get_logs(int tail, uint8_t **out,
                                        size_t *out_len,
                                        const char **error_message)
{
    if (!out || !out_len)
    {
        if (error_message)
            *error_message = "invalid output";
        return WAIOT_HTTP_INTERNAL_ERROR;
    }

    *out = NULL;
    *out_len = 0;
    if (tail < 0)
    {
        tail = 0;
    }

    int filled = log_get_filled();
    if (filled < 0)
    {
        if (error_message)
            *error_message = "log error";
        return WAIOT_HTTP_INTERNAL_ERROR;
    }

    int to_send = filled;
    if (tail > 0 && tail < filled)
    {
        to_send = tail;
    }

    uint8_t *buf = (uint8_t *)malloc((size_t)to_send + 1);
    if (!buf)
    {
        if (error_message)
            *error_message = "OOM";
        return WAIOT_HTTP_INTERNAL_ERROR;
    }

    int offset = filled - to_send;
    int n = log_read_from_oldest((uint32_t)offset, buf, (uint32_t)to_send);
    if (n < 0)
    {
        free(buf);
        if (error_message)
            *error_message = "read error";
        return WAIOT_HTTP_INTERNAL_ERROR;
    }

    buf[n] = '\0';
    *out = buf;
    *out_len = (size_t)n;
    return WAIOT_HTTP_OK;
}

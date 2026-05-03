#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

httpd_handle_t waiot_start_http_server(void);
void waiot_stop_http_server(httpd_handle_t server);

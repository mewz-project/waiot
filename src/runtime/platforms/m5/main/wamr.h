#pragma once

#include "esp_http_server.h"

void init_wamr();
void *run_wamr();
void stop_current_wasm_instance();
void launch_new_wasm_instance();
esp_err_t receive_wasm_binary(httpd_req_t *req);
bool is_wasm_instance_running();
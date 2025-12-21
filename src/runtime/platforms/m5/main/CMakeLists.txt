# Copyright (C) 2021 Intel Corporation and others.  All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

idf_component_register(SRCS "main.c" "utils.c" "wasi.c" "wamr.c" "wasi_m5.cc"
                    INCLUDE_DIRS "."
                    REQUIRES esp_http_server esp_wifi esp_netif nvs_flash esp_driver_gpio driver)

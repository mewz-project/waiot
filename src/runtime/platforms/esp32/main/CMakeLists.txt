# Copyright (C) 2021 Intel Corporation and others.  All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set(SRCS "main.c" "wasi.c" "wamr.c" "config.c" "pinmap.c")

set(REQUIRES esp_http_server esp_http_client esp_wifi esp_netif nvs_flash esp_driver_gpio driver json mbedtls)

set(INCLUDE_DIRS "." "../../../../../wasm-micro-runtime")

if (CONFIG_USE_TFLM)
    message(STATUS "### WASI-NN with TensorFlow Lite Micro is enabled")
    list(APPEND SRCS "wasi_nn.cc")
    list(APPEND REQUIRES esp-tflite-micro)
    set(TFLM_DIR "../managed_components/espressif__esp-tflite-micro")
    list(APPEND INCLUDE_DIRS "${TFLM_DIR}" "${TFLM_DIR}/third_party/gemmlowp"
                         "${TFLM_DIR}/third_party/flatbuffers/include"
                         "${TFLM_DIR}/third_party/ruy"
                         "${TFLM_DIR}/third_party/kissfft")
    idf_build_set_property(COMPILE_DEFINITIONS "TF_LITE_STATIC_MEMORY=1" APPEND)
endif()

idf_component_register(SRCS ${SRCS}
                    INCLUDE_DIRS ${INCLUDE_DIRS}
                    REQUIRES ${REQUIRES})
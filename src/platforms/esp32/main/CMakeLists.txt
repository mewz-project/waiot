# Copyright (C) 2021 Intel Corporation and others.  All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set(CORE_DIR "../../../core")

set(SRCS
    "main.c"
    "esp_http.c"
    "wasi.c"
    "platform_wamr.c"
    "log.c"
    "config.c"
    "${CORE_DIR}/http.c"
    "${CORE_DIR}/pinmap.c"
    "${CORE_DIR}/wamr.c")

set(REQUIRES esp_http_server esp_http_client esp_wifi esp_netif nvs_flash esp_driver_gpio driver json)

set(INCLUDE_DIRS "." "${CORE_DIR}" "../../../../wasm-micro-runtime")

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

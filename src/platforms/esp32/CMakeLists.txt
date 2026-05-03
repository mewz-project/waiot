# Copyright (C) 2019-21 Intel Corporation and others.  All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.14)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(wamr-simple)

set(NVS_DATA_CSV "${CMAKE_CURRENT_LIST_DIR}/nvs_data.csv")
if(EXISTS "${NVS_DATA_CSV}")
    nvs_create_partition_image(config "${NVS_DATA_CSV}" FLASH_IN_PROJECT)
else()
    message(WARNING "nvs_data.csv not found; skipping config partition image generation")
endif()

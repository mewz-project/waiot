#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void wamr_set_allocator(void *malloc_func, void *realloc_func, void *free_func);
void init_wamr(void);
int wamr_set_wasm_binary(const uint8_t *data, size_t size);
void stop_current_wasm_instance(void);
void launch_new_wasm_instance(void);
bool is_wasm_instance_running(void);

void waiot_platform_register_extra_natives(void);
void waiot_platform_print_memory_info(void);

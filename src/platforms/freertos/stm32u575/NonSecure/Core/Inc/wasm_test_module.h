#ifndef WASM_TEST_MODULE_H
#define WASM_TEST_MODULE_H

#include <stddef.h>
#include <stdint.h>

#define WASM_TEST_MODULE_SIZE 43U

extern const uint8_t g_wasm_test_module[WASM_TEST_MODULE_SIZE];
extern const size_t g_wasm_test_module_size;

#endif /* WASM_TEST_MODULE_H */

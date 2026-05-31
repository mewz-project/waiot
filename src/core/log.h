#pragma once

#include <stdint.h>

int32_t log_append(const uint8_t *data, uint32_t len);
int32_t log_get_filled(void);
int32_t log_read_from_oldest(uint32_t offset, uint8_t *out, uint32_t len);

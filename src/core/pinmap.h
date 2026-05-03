#pragma once

#include <stdint.h>

#define MAX_GPIO 64

int32_t map_pin(int32_t virtual_pin);
void pinmap_reset_identity(void);
int pinmap_set_virtual_to_physical(int32_t virtual_pin, int32_t physical_pin);

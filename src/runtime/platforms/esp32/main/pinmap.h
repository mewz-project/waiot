#pragma once

#include <stdint.h>

#define MAX_GPIO 64

int32_t map_pin(int32_t virtual_pin);

// Pin mapping management API
void pinmap_reset_identity(void);

// Map virtual pin directly to physical pin number
// Returns: 0=OK, -1=invalid arguments (out of range)
int pinmap_set_virtual_to_physical(int32_t virtual_pin, int32_t physical_pin);

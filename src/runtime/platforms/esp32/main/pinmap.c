#include "pinmap.h"

#include <stdlib.h>
#include <string.h>

int32_t *g_pin_mapping = NULL;

int32_t map_pin(int32_t virtual_pin)
{
    if (g_pin_mapping == NULL)
    {
        return virtual_pin;
    }
    if (virtual_pin < 0 || virtual_pin >= MAX_GPIO)
    {
        return virtual_pin;
    }
    int32_t mapped = g_pin_mapping[virtual_pin];
    return (mapped >= 0) ? mapped : virtual_pin;
}

void pinmap_reset_identity(void)
{
    if (!g_pin_mapping)
    {
        g_pin_mapping = (int32_t *)malloc(sizeof(int32_t) * MAX_GPIO);
        if (!g_pin_mapping)
            return;
    }
    for (int32_t i = 0; i < MAX_GPIO; i++)
    {
        g_pin_mapping[i] = i;
    }
}

int pinmap_set_virtual_to_physical(int32_t virtual_pin, int32_t physical_pin)
{
    if (virtual_pin < 0 || virtual_pin >= MAX_GPIO)
        return -1;
    if (physical_pin < 0 || physical_pin >= MAX_GPIO)
        return -1;
    if (!g_pin_mapping)
    {
        pinmap_reset_identity();
    }
    g_pin_mapping[virtual_pin] = physical_pin;
    return 0;
}

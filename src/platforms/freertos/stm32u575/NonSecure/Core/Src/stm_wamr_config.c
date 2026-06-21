#include "platform_wamr.h"

#include "wamr.h"

#include <stdio.h>
#include <stdlib.h>

void waiot_platform_configure_wamr(void)
{
  wamr_set_allocator((void *)malloc, (void *)realloc, (void *)free);
}

void waiot_platform_print_memory_info(void)
{
  printf("waiot: memory info unavailable\r\n");
}

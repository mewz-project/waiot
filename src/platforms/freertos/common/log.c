#include "log.h"

#include <stddef.h>
#include <string.h>

#define LOG_RING_BUF_SIZE (8 * 1024)

static uint8_t s_log_ring[LOG_RING_BUF_SIZE];
static size_t s_log_write_pos = 0;
static size_t s_log_filled = 0;

int32_t log_append(const uint8_t *data, uint32_t len)
{
  if (!data && len > 0)
  {
    return -1;
  }

  while (len > 0)
  {
    size_t to_end = LOG_RING_BUF_SIZE - s_log_write_pos;
    size_t chunk = len < to_end ? len : to_end;
    memcpy(s_log_ring + s_log_write_pos, data, chunk);
    s_log_write_pos = (s_log_write_pos + chunk) % LOG_RING_BUF_SIZE;
    if (s_log_filled + chunk >= LOG_RING_BUF_SIZE)
    {
      s_log_filled = LOG_RING_BUF_SIZE;
    }
    else
    {
      s_log_filled += chunk;
    }
    data += chunk;
    len -= (uint32_t)chunk;
  }

  return 0;
}

int32_t log_get_filled(void)
{
  return (int32_t)s_log_filled;
}

int32_t log_read_from_oldest(uint32_t offset, uint8_t *out, uint32_t len)
{
  if (offset > s_log_filled || (!out && len > 0))
  {
    return -1;
  }

  uint32_t can_read = (uint32_t)s_log_filled - offset;
  if (len > can_read)
  {
    len = can_read;
  }
  if (len == 0)
  {
    return 0;
  }

  uint32_t oldest = (uint32_t)((s_log_write_pos + LOG_RING_BUF_SIZE -
                                s_log_filled) %
                               LOG_RING_BUF_SIZE);
  uint32_t start = (oldest + offset) % LOG_RING_BUF_SIZE;
  uint32_t to_end = LOG_RING_BUF_SIZE - start;
  uint32_t chunk1 = len < to_end ? len : to_end;
  memcpy(out, s_log_ring + start, chunk1);
  if (chunk1 < len)
  {
    memcpy(out + chunk1, s_log_ring, len - chunk1);
  }
  return (int32_t)len;
}

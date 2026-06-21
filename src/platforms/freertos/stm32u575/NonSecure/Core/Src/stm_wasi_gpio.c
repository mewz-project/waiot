#include "wasi.h"

#include "main.h"
#include "pinmap.h"

#include <stdint.h>

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
} stm_gpio_pin_t;

static void enable_gpio_clock(GPIO_TypeDef *port)
{
  if (port == GPIOA)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
  }
  else if (port == GPIOB)
  {
    __HAL_RCC_GPIOB_CLK_ENABLE();
  }
  else if (port == GPIOC)
  {
    __HAL_RCC_GPIOC_CLK_ENABLE();
  }
  else if (port == GPIOD)
  {
    __HAL_RCC_GPIOD_CLK_ENABLE();
  }
  else if (port == GPIOE)
  {
    __HAL_RCC_GPIOE_CLK_ENABLE();
  }
  else if (port == GPIOF)
  {
    __HAL_RCC_GPIOF_CLK_ENABLE();
  }
  else if (port == GPIOG)
  {
    HAL_PWREx_EnableVddIO2();
    __HAL_RCC_GPIOG_CLK_ENABLE();
  }
  else if (port == GPIOH)
  {
    __HAL_RCC_GPIOH_CLK_ENABLE();
  }
}

static int decode_gpio_pin(int32_t encoded, stm_gpio_pin_t *out)
{
  if (!out || encoded < 0)
  {
    return -1;
  }

  int32_t port_index = encoded / 16;
  int32_t pin_index = encoded % 16;
  if (pin_index < 0 || pin_index > 15)
  {
    return -1;
  }

  switch (port_index)
  {
  case 0:
    out->port = GPIOA;
    break;
  case 1:
    out->port = GPIOB;
    break;
  case 2:
    out->port = GPIOC;
    break;
  case 3:
    out->port = GPIOD;
    break;
  case 4:
    out->port = GPIOE;
    break;
  case 5:
    out->port = GPIOF;
    break;
  case 6:
    out->port = GPIOG;
    break;
  case 7:
    out->port = GPIOH;
    break;
  default:
    return -1;
  }

  out->pin = (uint16_t)(1U << (uint32_t)pin_index);
  return 0;
}

static int is_reserved_gpio(GPIO_TypeDef *port, uint16_t pin)
{
  if (port == GPIOA && (pin == GPIO_PIN_9 || pin == GPIO_PIN_10))
  {
    return 1;
  }
  if (port == GPIOG && pin == GPIO_PIN_2)
  {
    return 1;
  }
  return 0;
}

int32_t gpio_set_pin_mode(wasm_exec_env_t exec_env, int32_t pin, int32_t dir)
{
  (void)exec_env;

  stm_gpio_pin_t gpio;
  if (decode_gpio_pin(map_pin(pin), &gpio) != 0 ||
      is_reserved_gpio(gpio.port, gpio.pin))
  {
    return -1;
  }

  GPIO_InitTypeDef init = {0};
  init.Pin = gpio.pin;
  init.Mode = (dir == 0) ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT_PP;
  init.Pull = GPIO_NOPULL;
  init.Speed = GPIO_SPEED_FREQ_LOW;

  enable_gpio_clock(gpio.port);
  HAL_GPIO_Init(gpio.port, &init);
  return 0;
}

int32_t gpio_read(wasm_exec_env_t exec_env, int32_t pin)
{
  (void)exec_env;

  stm_gpio_pin_t gpio;
  if (decode_gpio_pin(map_pin(pin), &gpio) != 0 ||
      is_reserved_gpio(gpio.port, gpio.pin))
  {
    return -1;
  }

  return (HAL_GPIO_ReadPin(gpio.port, gpio.pin) == GPIO_PIN_SET) ? 1 : 0;
}

int32_t gpio_write(wasm_exec_env_t exec_env, int32_t pin, int32_t value)
{
  (void)exec_env;

  stm_gpio_pin_t gpio;
  if (decode_gpio_pin(map_pin(pin), &gpio) != 0 ||
      is_reserved_gpio(gpio.port, gpio.pin))
  {
    return -1;
  }

  HAL_GPIO_WritePin(gpio.port, gpio.pin,
                    value ? GPIO_PIN_SET : GPIO_PIN_RESET);
  return 0;
}

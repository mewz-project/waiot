/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Secure/Src/secure_nsc.c
  * @author  MCD Application Team
  * @brief   This file contains the non-secure callable APIs (secure world)
  ******************************************************************************
    * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* USER CODE BEGIN Non_Secure_CallLib */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "secure_nsc.h"
/** @addtogroup STM32U5xx_HAL_Examples

  * @{
  */

/** @addtogroup Templates
  * @{
  */

/* Global variables ----------------------------------------------------------*/
void *pSecureFaultCallback = NULL;   /* Pointer to secure fault callback in Non-secure */
void *pSecureErrorCallback = NULL;   /* Pointer to secure error callback in Non-secure */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define SECURE_LED_BLINK_DELAY  400000U
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void SECURE_LED_Delay(volatile uint32_t count);
/* Private functions ---------------------------------------------------------*/
static void SECURE_LED_Delay(volatile uint32_t count)
{
  while (count-- > 0U)
  {
    __NOP();
  }
}

/**
  * @brief  Secure registration of non-secure callback.
  * @param  CallbackId  callback identifier
  * @param  func        pointer to non-secure function
  * @retval None
  */
    CMSE_NS_ENTRY void SECURE_RegisterCallback(SECURE_CallbackIDTypeDef CallbackId, void *func)
    {
      if(func != NULL)
      {
        switch(CallbackId)
        {
          case SECURE_FAULT_CB_ID:           /* SecureFault Interrupt occurred */
          pSecureFaultCallback = func;
          break;
          case GTZC_ERROR_CB_ID:             /* GTZC Interrupt occurred */
          pSecureErrorCallback = func;
          break;
          default:
          /* unknown */
          break;
        }
      }
    }

/**
  * @brief  Blink the Secure-owned red LED once.
  * @retval None
  */
CMSE_NS_ENTRY void SECURE_LED_BlinkOnce(void)
{
  SECURE_LED_On();
  SECURE_LED_Delay(SECURE_LED_BLINK_DELAY);
  SECURE_LED_Off();
  SECURE_LED_Delay(SECURE_LED_BLINK_DELAY);
}

/**
  * @}
  */

/**
  * @}
  */
/* USER CODE END Non_Secure_CallLib */


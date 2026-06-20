/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
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

/* Includes ------------------------------------------------------------------*/
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "wasm_export.h"
#include "wasm_test_module.h"

#include <stdbool.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define WAMR_TASK_STACK_WORDS        2048U
#define WAMR_GLOBAL_HEAP_SIZE        (64U * 1024U)
#define WAMR_MODULE_STACK_SIZE       (2U * 1024U)
#define WAMR_MODULE_HEAP_SIZE        (2U * 1024U)
#define WAMR_ERROR_BUFFER_SIZE       128U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static StaticTask_t wamrTaskControlBlock;
static StackType_t wamrTaskStack[WAMR_TASK_STACK_WORDS];
static uint8_t wamrGlobalHeap[WAMR_GLOBAL_HEAP_SIZE] __attribute__((aligned(8)));
static uint8_t wamrModuleBuffer[WASM_TEST_MODULE_SIZE] __attribute__((aligned(4)));

static osThreadId_t wamrTaskHandle;
static const osThreadAttr_t wamrTask_attributes = {
  .name = "wamrTask",
  .priority = (osPriority_t) osPriorityBelowNormal,
  .stack_mem = wamrTaskStack,
  .stack_size = sizeof(wamrTaskStack),
  .cb_mem = &wamrTaskControlBlock,
  .cb_size = sizeof(wamrTaskControlBlock)
};

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void StartWamrTask(void *argument);
static bool RunWamrTestModule(void);
static void ReportWamrResult(bool success);

/* USER CODE END FunctionPrototypes */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  wamrTaskHandle = osThreadNew(StartWamrTask, NULL, &wamrTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}
/* USER CODE BEGIN Header_StartDefaultTask */
/**
* @brief Function implementing the defaultTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN defaultTask */
  TickType_t previous_wake_time = xTaskGetTickCount();

  /* Infinite loop */
  for(;;)
  {
    BSP_LED_Toggle(LED_GREEN);
    vTaskDelayUntil(&previous_wake_time, pdMS_TO_TICKS(500));
  }
  /* USER CODE END defaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static void StartWamrTask(void *argument)
{
  (void)argument;

  printf("call WAMR\r\n");
  bool success = RunWamrTestModule();
  ReportWamrResult(success);

  for (;;)
  {
    osDelay(1000);
  }
}

static bool RunWamrTestModule(void)
{
  RuntimeInitArgs init_args;
  wasm_module_t module = NULL;
  wasm_module_inst_t module_inst = NULL;
  wasm_exec_env_t exec_env = NULL;
  wasm_function_inst_t function = NULL;
  char error_buf[WAMR_ERROR_BUFFER_SIZE] = { 0 };
  uint32_t argv[1] = { 0 };
  bool success = false;

  memset(&init_args, 0, sizeof(init_args));
  init_args.mem_alloc_type = Alloc_With_Pool;
  init_args.mem_alloc_option.pool.heap_buf = wamrGlobalHeap;
  init_args.mem_alloc_option.pool.heap_size = sizeof(wamrGlobalHeap);
  init_args.running_mode = Mode_Interp;

  memcpy(wamrModuleBuffer, g_wasm_test_module, g_wasm_test_module_size);

  printf("WAMR: init\r\n");
  if (!wasm_runtime_full_init(&init_args))
  {
    printf("WAMR: init failed\r\n");
    printf("exit WAMR: failure\r\n");
    return false;
  }

  wasm_runtime_set_log_level(WASM_LOG_LEVEL_WARNING);

  module = wasm_runtime_load(wamrModuleBuffer, g_wasm_test_module_size,
                             error_buf, sizeof(error_buf));
  if (module == NULL)
  {
    printf("WAMR: load failed: %s\r\n", error_buf);
    goto cleanup;
  }

  module_inst = wasm_runtime_instantiate(module, WAMR_MODULE_STACK_SIZE,
                                         WAMR_MODULE_HEAP_SIZE, error_buf,
                                         sizeof(error_buf));
  if (module_inst == NULL)
  {
    printf("WAMR: instantiate failed: %s\r\n", error_buf);
    goto cleanup;
  }

  exec_env = wasm_runtime_create_exec_env(module_inst, WAMR_MODULE_STACK_SIZE);
  if (exec_env == NULL)
  {
    printf("WAMR: create exec env failed\r\n");
    goto cleanup;
  }

  function = wasm_runtime_lookup_function(module_inst, "run");
  if (function == NULL)
  {
    printf("WAMR: function not found\r\n");
    goto cleanup;
  }

  if (!wasm_runtime_call_wasm(exec_env, function, 0, argv))
  {
    const char *exception = wasm_runtime_get_exception(module_inst);
    printf("WAMR: call failed: %s\r\n", exception != NULL ? exception : "");
    goto cleanup;
  }

  printf("WAMR: run() returned %lu\r\n", (unsigned long)argv[0]);
  success = (argv[0] == 42U);

cleanup:
  printf("exit WAMR: %s\r\n", success ? "success" : "failure");

  if (exec_env != NULL)
  {
    wasm_runtime_destroy_exec_env(exec_env);
  }
  if (module_inst != NULL)
  {
    wasm_runtime_deinstantiate(module_inst);
  }
  if (module != NULL)
  {
    wasm_runtime_unload(module);
  }
  wasm_runtime_destroy();

  return success;
}

static void ReportWamrResult(bool success)
{
  if (success)
  {
    for (uint32_t i = 0; i < 6U; i++)
    {
      BSP_LED_Toggle(LED_BLUE);
      osDelay(120);
    }
  }
  else
  {
    for (;;)
    {
      SECURE_LED_BlinkOnce();
      osDelay(500);
    }
  }
}

/* USER CODE END Application */


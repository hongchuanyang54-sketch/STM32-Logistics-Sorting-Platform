/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for TaskDisplay */
osThreadId_t TaskDisplayHandle;
const osThreadAttr_t TaskDisplay_attributes = {
  .name = "TaskDisplay",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for TaskKey */
osThreadId_t TaskKeyHandle;
const osThreadAttr_t TaskKey_attributes = {
  .name = "TaskKey",
  .stack_size = 192 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for TaskUartRx */
osThreadId_t TaskUartRxHandle;
const osThreadAttr_t TaskUartRx_attributes = {
  .name = "TaskUartRx",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for TaskUartTx */
osThreadId_t TaskUartTxHandle;
const osThreadAttr_t TaskUartTx_attributes = {
  .name = "TaskUartTx",
  .stack_size = 192 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartTaskDisplay(void *argument);
void StartTaskKey(void *argument);
void StartTaskUartRx(void *argument);
void StartTaskUartTx(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
   /* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created. It is also called by various parts of the
   demo application. If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
}
/* USER CODE END 5 */

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

  /* Create the thread(s) */
  /* creation of TaskDisplay */
  TaskDisplayHandle = osThreadNew(StartTaskDisplay, NULL, &TaskDisplay_attributes);

  /* creation of TaskKey */
  TaskKeyHandle = osThreadNew(StartTaskKey, NULL, &TaskKey_attributes);

  /* creation of TaskUartRx */
  TaskUartRxHandle = osThreadNew(StartTaskUartRx, NULL, &TaskUartRx_attributes);

  /* creation of TaskUartTx */
  TaskUartTxHandle = osThreadNew(StartTaskUartTx, NULL, &TaskUartTx_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartTaskDisplay */
/**
  * @brief  Function implementing the TaskDisplay thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartTaskDisplay */
void StartTaskDisplay(void *argument)
{
  /* USER CODE BEGIN StartTaskDisplay */
  /* 本阶段先空转，等心跳验证通过后再填业务逻辑 */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartTaskDisplay */
}

/* USER CODE BEGIN Header_StartTaskKey */
/**
* @brief Function implementing the TaskKey thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskKey */
void StartTaskKey(void *argument)
{
  /* USER CODE BEGIN StartTaskKey */
  /* 本阶段先空转，等心跳验证通过后再填业务逻辑 */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartTaskKey */
}

/* USER CODE BEGIN Header_StartTaskUartRx */
/**
* @brief Function implementing the TaskUartRx thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskUartRx */
void StartTaskUartRx(void *argument)
{
  /* USER CODE BEGIN StartTaskUartRx */
  /* 本阶段先空转，等心跳验证通过后再填业务逻辑 */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartTaskUartRx */
}

/* USER CODE BEGIN Header_StartTaskUartTx */
/**
* @brief Function implementing the TaskUartTx thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskUartTx */
void StartTaskUartTx(void *argument)
{
  /* USER CODE BEGIN StartTaskUartTx */
  static const char msg[] = "tick\r\n";

  for(;;)
  {
    /* 心跳：调度器活着的最直接证据 —— 串口每 500ms 打一个 tick。
       正确写法是 osDelay(500)，单位是 tick；本工程 TICK_RATE_HZ=1000，
       所以 500 tick 正好 = 500ms。 */
    HAL_UART_Transmit(&huart1, (uint8_t *)msg, sizeof(msg) - 1, 100);
    osDelay(500);
  }
  /* USER CODE END StartTaskUartTx */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */


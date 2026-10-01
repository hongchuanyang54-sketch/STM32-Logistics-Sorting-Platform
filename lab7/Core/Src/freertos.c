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
/* 诊断用，定义在 main.c；定位完会删 */
#include "usart.h"
#include "display.h"
#include "key.h"
#include "uart_send.h"
extern void Counter_ApplyCommand(const char *line);

/* 计数器由 main.c 持有，键任务和串口任务都要用它 */
extern uint8_t Counter_Get(void);
extern void    Counter_Set(int value);


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
/* Definitions for uartRxQueue */
osMessageQueueId_t uartRxQueueHandle;
const osMessageQueueAttr_t uartRxQueue_attributes = {
  .name = "uartRxQueue"
};
/* Definitions for uartEvtQueue */
osMessageQueueId_t uartEvtQueueHandle;
const osMessageQueueAttr_t uartEvtQueue_attributes = {
  .name = "uartEvtQueue"
};
/* Definitions for counterMutex */
osMutexId_t counterMutexHandle;
const osMutexAttr_t counterMutex_attributes = {
  .name = "counterMutex"
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
   Error_Handler();
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
   Error_Handler();
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
  /* Create the mutex(es) */
  /* creation of counterMutex */
  counterMutexHandle = osMutexNew(&counterMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of uartRxQueue */
  uartRxQueueHandle = osMessageQueueNew (32, sizeof(uint8_t), &uartRxQueue_attributes);

  /* creation of uartEvtQueue */
  uartEvtQueueHandle = osMessageQueueNew (8, sizeof(UartEvent_t), &uartEvtQueue_attributes);

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
  /* Display_ShowNum 一次调用扫描一轮（十位 1ms + 个位 1ms）。
     本任务优先级最高，保证扫描节拍不被别人拖慢 —— 刷新低于 ~50Hz 肉眼就见闪。
     函数内部用 osDelay 让出 CPU，所以不会饿死下面的任务。 */
  for(;;)
  {
    Display_ShowNum(Counter_Get());
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
  for(;;)
  {
    /* 只有计数值确实改变时才上报，
       否则到 99 后长按会向串口刷屏、上位机镜像计数也会跑偏 */
    switch (Key_Process())
    {
      case KEY_EVENT_ADD:
        if (Counter_Get() < 99) { Counter_Set(Counter_Get() + 1); Uart_EventPush(UART_EVT_ADD); }
        break;

      case KEY_EVENT_SUB:
        if (Counter_Get() > 0)  { Counter_Set(Counter_Get() - 1); Uart_EventPush(UART_EVT_SUB); }
        break;

      case KEY_EVENT_CLEAR:
        Counter_Set(0);
        Uart_EventPush(UART_EVT_ZERO);
        break;

      default:
        break;
    }

    /* 10ms 轮询一次足够：人的按键反应在 100ms 以上 */
    osDelay(10);
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
  /* 任务私有的攒帧缓冲。字节是队列一个个送过来的，
     这里不需要双缓冲、不需要关中断 —— 队列已经保证了原子性。 */
  static char    line[50];
  static uint8_t idx = 0;
  uint8_t b;

  for(;;)
  {
    /* 阻塞等一个字节。没有数据时本任务完全不被调度，不占 CPU ——
       这是队列相比裸机版轮询标志位最本质的差别。 */
    if (osMessageQueueGet(uartRxQueueHandle, &b, NULL, osWaitForever) != osOK)
    {
      continue;
    }


    if (b == '\r' || b == '\n')
    {
      if (idx > 0)                     /* 一行结束，交给解析 */
      {
        line[idx] = '\0';
        Counter_ApplyCommand(line);
        idx = 0;
      }
    }
    else if (idx < sizeof(line) - 1)
    {
      line[idx++] = (char)b;
    }
    else
    {
      idx = 0;                         /* 过长的帧直接丢弃，从头再来 */
    }
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
  UartEvent_t evt;

  for(;;)
  {
    /* 阻塞等一个待上报事件。相比裸机版每轮主循环都去查一次"有没有事件"，
       这里任务是真的睡着了，由内核在入队那一刻唤醒。 */
    if (osMessageQueueGet(uartEvtQueueHandle, &evt, NULL, osWaitForever) == osOK)
    {
      Uart_SendEvent(evt);
    }
  }
  /* USER CODE END StartTaskUartTx */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */


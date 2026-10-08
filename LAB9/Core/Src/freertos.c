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
#include "uart_rx.h"

/* main.c 里的应用逻辑。任务体只负责按节拍调用它，
   业务代码留在 main.c，CubeMX 重新生成时不会被冲掉。 */
extern void App_ProcessFrame(const char *line);

/* main.c 里的串口单字节接收缓冲（定义在 main.c 的 USER CODE PV） */
extern uint8_t rx_byte;
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
/* Definitions for TaskUartRx */
osThreadId_t TaskUartRxHandle;
const osThreadAttr_t TaskUartRx_attributes = {
  .name = "TaskUartRx",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for TaskDisplay */
osThreadId_t TaskDisplayHandle;
const osThreadAttr_t TaskDisplay_attributes = {
  .name = "TaskDisplay",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for uartRxQueue */
osMessageQueueId_t uartRxQueueHandle;
const osMessageQueueAttr_t uartRxQueue_attributes = {
  .name = "uartRxQueue"
};
/* Definitions for frameQueue */
osMessageQueueId_t frameQueueHandle;
const osMessageQueueAttr_t frameQueue_attributes = {
  .name = "frameQueue"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartTaskUartRx(void *argument);
void StartTaskDisplay(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* 任务栈溢出了。停在这里而不是继续跑，
      否则踩坏的是别的任务的数据，症状会离现场很远。 */
   (void)xTask;
   (void)pcTaskName;
   Error_Handler();
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
   /* 堆不够用了（configTOTAL_HEAP_SIZE 配小了）。
      CubeMX 的 FreeRTOS 界面里能看到 TOTAL_HEAP_USED，
      加任务/队列之后记得回头核一下。 */
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
  uartRxQueueHandle = osMessageQueueNew (64, sizeof(uint8_t), &uartRxQueue_attributes);

  /* creation of frameQueue */
  frameQueueHandle = osMessageQueueNew (3, sizeof(RxFrame_t), &frameQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of TaskUartRx */
  TaskUartRxHandle = osThreadNew(StartTaskUartRx, NULL, &TaskUartRx_attributes);

  /* creation of TaskDisplay */
  TaskDisplayHandle = osThreadNew(StartTaskDisplay, NULL, &TaskDisplay_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */

  /* 串口接收放在这里使能，而不是 main() 的 USER CODE 2 里。
     为什么：中断回调要把字节投进 uartRxQueue，而队列是上面刚建好的。
     如果提前到 main() 里使能，从"中断打开"到"队列建好"之间有个窗口，
     这期间到达的字节会因为句柄还是 NULL 而被 osMessageQueuePut 直接丢掉
     （CMSIS 包装层返回 osErrorParameter，不会崩，但字节没了）。

     放在这里之后，即使调度器还没启动，字节也能正常入队，
     TaskUartRx 起来后照样能取到 —— 一个都不丢。 */
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartTaskUartRx */
/**
  * @brief  Function implementing the TaskUartRx thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartTaskUartRx */
void StartTaskUartRx(void *argument)//这个任务是用来接收串口数据的，接收到的数据会被放入队列中
{
  /* USER CODE BEGIN StartTaskUartRx */
  /* 任务私有：凑满的整帧先放这儿，再投进 frameQueue。
     必须写成 static —— 它是 256 字节，放栈上会把本任务的
     128 words（512 字节）栈直接撑爆。 */
  static RxFrame_t frame;//作用是存储接收到的完整帧
  uint8_t          b;//作用是存储接收到的单个字节

  /* 本任务只做"搬字节 + 攒帧"，很快，所以优先级给 AboveNormal：
     STM32F1 的 UART 没有 FIFO，收一个字节就要进一次中断，
     不能让别的任务把字节在队列里堆着不管。 */
  for(;;)
  {
    /* 阻塞等一个字节。没有数据时本任务完全不被调度，不占 CPU */
    if (osMessageQueueGet(uartRxQueueHandle, &b, NULL, osWaitForever) != osOK)
    {
      continue;
    }

    /* 攒帧：'}' 与 '\n' 双标记判定，凑满一帧就投给显示任务 */
    if (UartRx_Feed(b, &frame))//作用是把接收到的字节喂给攒帧状态机，凑满一帧就返回 1
    {
      /* 投递整帧给显示任务。osWaitForever 不行：显示任务在忙时不能把收字节的任务堵死。 */
      /* USER CODE BEGIN StartTaskUartRx_PutFrame */
      /* 不等待投递 —— 显示任务在忙时不能把收字节的任务堵死。 */
      if (osMessageQueuePut(frameQueueHandle, &frame, 0U, 0U) != osOK)//判断队列是否满了，如果满了就丢掉最旧的一帧，给最新的腾位置
      {
        /* 队列满了：丢掉【最旧】的一帧，给最新的腾位置。
           不能按默认行为丢最新的 —— 这块屏显示的是"当前分拣状态"，
           主机最后发的那条指令必须能显示出来。

           队列真会满吗？会。实际协议帧约 50~60 字节，9600 波特下传输
           约 60ms，而刷屏要 55~60ms —— 两者相当，主机连发时就会堆积。
           （按单帧 256 字节的上限去推"永远填不满"是错的。） */
        RxFrame_t dropped;
        (void)osMessageQueueGet(frameQueueHandle, &dropped, NULL, 0U);
        (void)osMessageQueuePut(frameQueueHandle, &frame, 0U, 0U);
      }
      /* USER CODE END StartTaskUartRx_PutFrame */
    }
  }
  /* USER CODE END StartTaskUartRx */
}

/* USER CODE BEGIN Header_StartTaskDisplay */
/**
* @brief Function implementing the TaskDisplay thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskDisplay */
void StartTaskDisplay(void *argument)
{
  /* USER CODE BEGIN StartTaskDisplay */
  /* 任务私有：从队列取出来的帧。同样是 256 字节，写 static 不占栈。 */
  static RxFrame_t frame;

  /* 最慢的任务：解析一帧 + 刷一整屏 OLED 约 60ms
     （软件 I²C 要清 1024 字节显存 + 画 4 个汉字 + 4 段字符串）。
     压到最低优先级，收字节的任务不会被它拖住。

     裸机版这里是主循环里的 ProcessFrame()：刷屏那 60ms 里到达的新帧，
     会被单缓冲 frame_buffer 直接覆盖掉（丢的是哪一帧完全看时序）。
     现在帧走队列，队列满时明确丢最旧的（见 TaskUartRx），
     主机连发时最后一条指令也一定能显示出来 —— 这就是本任务存在的理由。 */
  for(;;)
  {
    if (osMessageQueueGet(frameQueueHandle, &frame, NULL, osWaitForever) == osOK)
    {
      App_ProcessFrame(frame.text);
    }
  }
  /* USER CODE END StartTaskDisplay */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

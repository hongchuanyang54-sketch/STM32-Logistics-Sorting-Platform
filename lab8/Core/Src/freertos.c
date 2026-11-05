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
#include "cmsis_os.h"
#include "main.h"
#include "task.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "motor.h"
#include "uart_frame.h"
#include "uart_send.h"
#include "usart.h"
#include <string.h>

/* main.c 里的应用逻辑。任务体只按节拍调用它们，
   业务代码留在 main.c，CubeMX 重新生成时不会被冲掉。 */
extern void App_HandleLine(const char *line);
extern void App_SampleAdc(void);
extern void App_ReportSpeed(void);
extern void App_RefreshOled(void);

/* main.c 里的串口接收启动函数（内部持有 DMA 接收缓冲） */
extern HAL_StatusTypeDef Uart_StartReceive(void);
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 各任务的节拍（ms）。configTICK_RATE_HZ = 1000，所以 1 tick = 1ms。 */
#define ADC_PERIOD_MS 25
#define REPORT_PERIOD_MS 100
/* 电机节拍直接用 MOTOR_TICK_MS（15ms），见 MOTOR/motor.h */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for TaskMotor */
osThreadId_t TaskMotorHandle;
const osThreadAttr_t TaskMotor_attributes = {
    .name = "TaskMotor",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)osPriorityHigh,
};
/* Definitions for TaskAdc */
osThreadId_t TaskAdcHandle;
const osThreadAttr_t TaskAdc_attributes = {
    .name = "TaskAdc",
    .stack_size = 160 * 4,
    .priority = (osPriority_t)osPriorityAboveNormal,
};
/* Definitions for TaskUartRx */
osThreadId_t TaskUartRxHandle;
const osThreadAttr_t TaskUartRx_attributes = {
    .name = "TaskUartRx",
    .stack_size = 256 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};
/* Definitions for TaskReport */
osThreadId_t TaskReportHandle;
const osThreadAttr_t TaskReport_attributes = {
    .name = "TaskReport",
    .stack_size = 160 * 4,
    .priority = (osPriority_t)osPriorityBelowNormal,
};
/* Definitions for TaskUartTx */
osThreadId_t TaskUartTxHandle;
const osThreadAttr_t TaskUartTx_attributes = {
    .name = "TaskUartTx",
    .stack_size = 192 * 4,
    .priority = (osPriority_t)osPriorityBelowNormal,
};
/* Definitions for TaskOled */
osThreadId_t TaskOledHandle;
const osThreadAttr_t TaskOled_attributes = {
    .name = "TaskOled",
    .stack_size = 256 * 4,
    .priority = (osPriority_t)osPriorityLow,
};
/* Definitions for uartRxQueue */
osMessageQueueId_t uartRxQueueHandle;
const osMessageQueueAttr_t uartRxQueue_attributes = {.name = "uartRxQueue"};
/* Definitions for uartTxQueue */
osMessageQueueId_t uartTxQueueHandle;
const osMessageQueueAttr_t uartTxQueue_attributes = {.name = "uartTxQueue"};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartTaskMotor(void *argument);
void StartTaskAdc(void *argument);
void StartTaskUartRx(void *argument);
void StartTaskReport(void *argument);
void StartTaskUartTx(void *argument);
void StartTaskOled(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName) {
  /* 任务栈溢出了。停在这里而不是继续跑，
     否则踩坏的是别的任务的数据，症状会离现场很远。 */
  (void)xTask;
  (void)pcTaskName;
  Error_Handler();
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void) {
  /* 堆不够用了（configTOTAL_HEAP_SIZE 配小了）。
     CubeMX 的 FreeRTOS 界面里能看到 TOTAL_HEAP_USED，
     加任务/队列后记得回头核一下。 */
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
  /* ⚠ 本行由 CubeMX 生成，但已手工修改：元素类型从 uint8_t（单字节）
     改为 UartFrame_t（整帧），深度从 32 降到 4。
     若日后用 CubeMX 重新生成代码，请在 FREERTOS → Queues 里把
     uartRxQueue 的 Item Size 改为 66（= sizeof(UartFrame_t)）、
     Queue Size 改为 4，否则会退回旧的“32 × 1 字节”配置，
     TaskUartRx 取到的将是错误粒度的数据。 */
  uartRxQueueHandle = osMessageQueueNew(
      UART_FRAME_QUEUE_DEPTH, sizeof(UartFrame_t), &uartRxQueue_attributes);

  /* creation of uartTxQueue */
  uartTxQueueHandle =
      osMessageQueueNew(8, sizeof(UartTxMsg_t), &uartTxQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of TaskMotor */
  TaskMotorHandle = osThreadNew(StartTaskMotor, NULL, &TaskMotor_attributes);

  /* creation of TaskAdc */
  TaskAdcHandle = osThreadNew(StartTaskAdc, NULL, &TaskAdc_attributes);

  /* creation of TaskUartRx */
  TaskUartRxHandle = osThreadNew(StartTaskUartRx, NULL, &TaskUartRx_attributes);

  /* creation of TaskReport */
  TaskReportHandle = osThreadNew(StartTaskReport, NULL, &TaskReport_attributes);

  /* creation of TaskUartTx */
  TaskUartTxHandle = osThreadNew(StartTaskUartTx, NULL, &TaskUartTx_attributes);

  /* creation of TaskOled */
  TaskOledHandle = osThreadNew(StartTaskOled, NULL, &TaskOled_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */

  /* 串口接收放在这里使能，而不是 main() 的 USER CODE 2 里。
     为什么：中断回调要把字节投进 uartRxQueue，而队列是上面刚建好的。
     如果提前到 main() 里使能，从"中断打开"到"队列建好"之间有个几微秒窗口，
     这期间到达的字节会因为句柄还是 NULL 而被 osMessageQueuePut 直接丢掉
     （CMSIS 包装层会返回 osErrorParameter，不会崩，但字节没了），
     主机的第一帧就废了。

     放在这里之后，即使调度器还没启动，帧也能正常入队（队列深度 4 帧），
     TaskUartRx 起来后照样能取到 —— 一个都不丢。 */
  (void)Uart_StartReceive();
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */
}

/* USER CODE BEGIN Header_StartTaskMotor */
/**
 * @brief  Function implementing the TaskMotor thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskMotor */
void StartTaskMotor(void *argument) {
  /* USER CODE BEGIN StartTaskMotor */
  /* 全工程唯一要求节拍准的任务：电机斜坡的快慢直接取决于这个周期。
     它优先级最高，不会被 OLED（~0.1 秒一轮）之类拖慢 ——
     裸机版这一拍实际是 ~500ms，就是被阻塞式的 OLED_Refresh() 拖的。 */
  for (;;) {
    Motor_SmoothUpdate();
    osDelay(MOTOR_TICK_MS); // 作用是每隔 MOTOR_TICK_MS（15ms）调用一次
                            // Motor_SmoothUpdate()，实现电机的平滑变速。
  }
  /* USER CODE END StartTaskMotor */
}

/* USER CODE BEGIN Header_StartTaskAdc */
/**
 * @brief Function implementing the TaskAdc thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskAdc */
void StartTaskAdc(void *argument) {
  /* USER CODE BEGIN StartTaskAdc */
  /* 采样一次约几十微秒，25ms 的周期余量很大。
     UART 模式下这个任务只更新显示用的 adc_value，不再碰电机。 */
  for (;;) {
    App_SampleAdc();
    osDelay(ADC_PERIOD_MS);
  }
  /* USER CODE END StartTaskAdc */
}

/* USER CODE BEGIN Header_StartTaskUartRx */
/**
 * @brief Function implementing the TaskUartRx thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskUartRx */
void StartTaskUartRx(void *argument) {
  /* USER CODE BEGIN StartTaskUartRx */
  /* 队列里现在放的是【整帧】而不是单个字节：
     DMA 把字节搬进 uart_rx_buf，IDLE 中断触发时 HAL 一次性告诉我们
     这一帧有多少字节，回调里 memcpy 成 UartFrame_t 投递过来。
     因此这里不再需要"攒字节"的私有缓冲，也不需要判 idx 越界。 */
  UartFrame_t frame;

  for (;;) {
    /* 阻塞等一帧。没有数据时本任务完全不被调度，不占 CPU */
    if (osMessageQueueGet(uartRxQueueHandle, &frame, NULL, osWaitForever) !=
        osOK) {
      continue;
    }

    /* 补字符串结束符（回调只拷贝了 len 个字节，没有写 '\0'） */
    if (frame.len >= UART_FRAME_MAX) {
      frame.len = UART_FRAME_MAX - 1;
    }
    frame.data[frame.len] = '\0';

    /* 一帧里可能包含多行（主机连发两条命令、彼此间隔小于 1 个字符时间时
       IDLE 就不会把它们分开），所以按 CR/LF 逐行切，空行跳过。
       正常单条命令的场景下，这个循环只跑一次。 */
    char *p = frame.data;
    // 下面的操作是处理一帧串口数据，逐行切分并调用 App_HandleLine()
    // 处理每一行。
    while (*p != '\0') {
      char *nl = p;
      while (*nl != '\0' && *nl != '\r' &&
             *nl != '\n') { // 作用是找到当前行的结束位置，nl 指向 '\0'、'\r' 或
                            // '\n'。
        nl++;
      }

      char saved = *nl;
      *nl = '\0'; // 作用是暂时把当前行的结束符替换为 '\0'，方便处理当前行。
      if (*p != '\0') {
        App_HandleLine(p);
      }
      if (saved == '\0') {
        break;
      }
      p = nl + 1; // 为了批量处理多行，p 指向下一行的开头。
    }
  }
  /* USER CODE END StartTaskUartRx */
}

/* USER CODE BEGIN Header_StartTaskReport */
/**
 * @brief Function implementing the TaskReport thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskReport */
void StartTaskReport(void *argument) {
  /* USER CODE BEGIN StartTaskReport */
  /* 单独开一个任务，就是为了保证 100ms 的上报节拍不被 OLED 刷新拖慢。
     裸机版上报和 OLED 挤在同一个 100ms 分支里，实际周期是 OLED 的 ~500ms。

     App_ReportSpeed() 只往发送队列投递，不做阻塞发送，所以这一拍很准。 */
  for (;;) {
    App_ReportSpeed();
    osDelay(REPORT_PERIOD_MS);
  }
  /* USER CODE END StartTaskReport */
}

/* USER CODE BEGIN Header_StartTaskUartTx */
/**
 * @brief Function implementing the TaskUartTx thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskUartTx */
void StartTaskUartTx(void *argument) {
  /* USER CODE BEGIN StartTaskUartTx */
  UartTxMsg_t msg;

  /* 全工程唯一真正操作 huart1 的地方。
     命令应答（TaskUartRx 经 App_HandleLine）和周期上报（TaskReport）
     都只往队列里投递，所以两路输出不会在串口上互相踩。 */
  for (;;) {
    if (osMessageQueueGet(uartTxQueueHandle, &msg, NULL, osWaitForever) ==
        osOK) {
      HAL_UART_Transmit(&huart1, (uint8_t *)msg.text,
                        (uint16_t)strlen(msg.text), 100);
    }
  }
  /* USER CODE END StartTaskUartTx */
}

/* USER CODE BEGIN Header_StartTaskOled */
/**
 * @brief Function implementing the TaskOled thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTaskOled */
void StartTaskOled(void *argument) {
  /* USER CODE BEGIN StartTaskOled */
  /* 最慢的任务：软件 I²C 刷一屏 43 个字符 = 387 次 START/STOP 事务，
     即使把 I2C_Delay 提速到 20 之后也要 ~0.13 秒（原来 ~0.5 秒）。

     压到最低优先级当"填缝"：上面 5 个任务不跑的时候它才跑。
     中途被抢占是安全的 —— 被抢占的是【主机】，只是 SDA/SCL 维持原电平
     更久、节拍变慢，从机一直等着（见 SOFT_I2C/soft_i2c.h 的说明）。

     osDelay(1) 不是给刷新节拍用的（那由 I²C 本身的耗时决定），
     而是让最低优先级的任务定期放一次手，别把空闲任务饿死。 */
  for (;;) {
    App_RefreshOled();
    osDelay(1);
  }
  /* USER CODE END StartTaskOled */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

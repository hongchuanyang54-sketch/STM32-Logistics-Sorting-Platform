/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    main.c
 * @brief   光电计数器（lab7）
 *
 * 功能：
 *   1. 2 位共阳数码管显示计数值（0~99）
 *   2. KEY1/KEY2/KEY3（PA1/PA2/PA3）加 / 减 / 清零，支持长按连加连减
 *   3. 串口 USART1 接收 {"GoodsNumber":"x"}\r\n 远程设定计数值
 *   4. KEY4（PA0）急停、KEY_RE（PA6）复位，经串口上报 ESTOP / RESET
 *
 * 上报报文（均以 \r\n 结尾）：ADD / SUB / ZERO / ESTOP / RESET
 * 下发报文：                  {"GoodsNumber":"x"}\r\n
 *
 * 模块划分：
 *   KEY/key.c        按键消抖 + 长短按状态机
 *   DISPLAY/display.c 数码管动态扫描
 *   UART_SEND/uart_send.c 事件队列 + 串口上报
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "display.h"
#include "key.h"
#include "uart_send.h"
#include "at24c02.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* USER CODE BEGIN PV */
uint8_t GoodsNumber = 0; /* 当前计数值 0~99 */

uint8_t rx_byte;             /* 串口单字节接收缓冲（交给 HAL 使用） */

/* 下面两个内核对象由 CubeMX 在 freertos.c 里创建
   （FREERTOS -> Tasks and Queues -> Queues / Mutexes） */
extern osMessageQueueId_t uartRxQueueHandle;   /* 中断收字节 -> TaskUartRx */
extern osMutexId_t        counterMutexHandle;  /* 保护 GoodsNumber 的写入 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
void Counter_Set(int value);
uint8_t Counter_Get(void);
void Counter_ApplyCommand(const char *line);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief  统一设置计数值
 * @param  value 目标值，越界时钳位到 0~99
 * @note   按键与串口下行都经过本函数，保证钳位规则只有一处。
 *         注意入参是 int：atoi() 返回的可能超过 99，
 *         若先转成 uint8_t 再判断会发生回绕（例如 300 -> 44）。
 */
void Counter_Set(int value) {
  if (value < 0)
    value = 0;
  if (value > 99)
    value = 99;

  /* 写计数值要加锁：TaskKey（按键）和 TaskUartRx（串口下行）都会写它，
     两个任务同时"读-改-写"会丢掉一次更新。
     读（Counter_Get）不加锁 —— 单字节读写是原子的，而且显示任务是最高优先级，
     不该为了读一个字节去抢锁、拖慢数码管扫描。 */
  (void)osMutexAcquire(counterMutexHandle, osWaitForever);

  GoodsNumber = (uint8_t)value;

  /* 无条件写盘，不做"值没变就跳过"的优化。
     因为一旦某次写失败（总线毛刺、器件还在忙），RAM 和 EEPROM 就分叉了；
     若还按"值没变就不写"跳过，用户之后一直按清零也修不回来 ——
     因为 RAM 已经是 0，判断相等直接跳过，下次开机旧值又冒出来。
     无条件写让分叉在下次操作时自动愈合。代价是偶尔多一次 6ms 的写，
     24C02 有 100 万次擦写寿命，人工改数的频率下可以忽略。

     写盘要 ~6ms（含芯片 5ms 内部写周期），这段时间锁一直被持有，
     另一个写者会阻塞这么久 —— 计数值是人工改的，这个代价可以接受。
     必须留在锁内：否则两个任务可能交错写，EEPROM 里最终留下的是旧值。
     显示任务不碰这把锁，所以数码管扫描完全不受影响。 */
  (void)AT24C02_SaveCount(GoodsNumber);

  (void)osMutexRelease(counterMutexHandle);
}

/**
 * @brief  读取当前计数值
 * @note   单字节读写在 Cortex-M3 上是原子的，显示任务直接读不会读到半个值
 */
uint8_t Counter_Get(void) {
  return GoodsNumber;
}

/**
 * @brief  解析串口下行指令 {"GoodsNumber":"x"}
 * @param  line 一行完整报文（以 '\0' 结尾），由 TaskUartRx 攒好后传入
 * @note   与裸机版的区别：这里直接拿到一整行字符串，不需要再关中断拷贝缓冲区。
 *         字节是通过队列逐个传过来的，攒帧缓冲是 TaskUartRx 的私有变量，
 *         天然没有竞态 —— 原来那套"关中断 + memcpy 到 local_buf"可以整个删掉。
 */
void Counter_ApplyCommand(const char *line) {
  char num_str[10];

  /* 宽度必须写成 %9[...]：num_str 只有 10 字节（含结尾的 '\0'），
     不加宽度限制的话，上位机发个 {"GoodsNumber":"999999999999"} 就能
     顺着栈写出 40 多字节，砸掉本函数的返回地址。 */
  if (sscanf(line, "{\"GoodsNumber\":\"%9[^\"]\"}", num_str) == 1) {
    Counter_Set(atoi(num_str));
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  Display_Init();

  /* 上电先把上次存盘的计数值读回来。
     位置有讲究：必须在 MX_GPIO_Init() 之后（PA4/PA5 这时才配好开漏输出）。
     读操作本身不带延时，所以可以在调度器启动前调用 ——
     写操作里有 osDelay，那个只能在任务里调。 */
  {
    int saved = AT24C02_LoadCount();
    /* 空片（没写过）读回来是 0xFF，读失败返回 -1，都当 0 处理。
       这一步同时也是在自检：I²C 不通的话这里读回来就是 -1。 */
    GoodsNumber = (saved >= 0 && saved <= 99) ? (uint8_t)saved : 0;
  }

  /* 启动串口单字节中断接收：每收到一个字节触发一次 USART1 中断。
     中断里只把字节投进 uartRxQueue，攒帧与解析在 TaskUartRx 里做。 */
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

  /* 这里【不要】调 Display_ShowNum()：
     它内部有 osDelay，而调度器还没启动，属于非法调用
     （vTaskDelay 会去操作还不存在的 pxCurrentTCB / 延迟链表）。
     当前之所以没出事，是因为 port.c 里 uxCriticalNesting 的复位值是
     0xaaaaaaaa，vPortExitCritical 判定不为 0 就不解除中断屏蔽，
     于是那个 PendSV 一直挂起到调度器启动后才被消化 —— 靠的是运气。
     TaskDisplay 一启动就会显示同一个值，本来也不需要在这里先显示一次。 */
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  /* 控制权已经交给调度器：osKernelStart() 不会返回。
     原先的裸机主循环（按键状态机 / 串口解析 / 上报 / 数码管扫描）
     已经拆成 freertos.c 里的四个任务。 */
  while (1) {
  }
  /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief  串口1 接收完成回调（每收到一个字节触发一次）
 * @note   中断里只做一件事：把字节投进 uartRxQueue，立刻返回。
 *         以前在这里逐字节攒帧、判 
、置标志位，现在全交给 TaskUartRx。
 *         中断变短了，也不用再操心主循环什么时候来取。
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
    (void)osMessageQueuePut(uartRxQueueHandle, &rx_byte, 0U, 0U);
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1); /* 重新使能接收 */
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   分拣器控制器（LAB9）
  *
  * 功能：
  *   接收上位机下发的分拣指令，解析出四个方向各有哪些货物，
  *   在 OLED 上以「上 / 下 / 左 / 右 + 货物名」四行显示。
  *
  * 下行报文（USART1，9600 8N1）：
  *   {"Down":"GoodC,GoodD","Up":"GoodA","Left":"","Right":""}\r\n
  *   货物名前的 "Good" 前缀不区分大小写，显示前会被去掉。
  *
  * 帧完整性判定：只有同时见到 '}' 与行结束符 '\n' 才认为一帧完整，
  *               避免把半截 JSON 当成完整帧解析。
  *
  * USART2 作为调试输出口，上电打印一次 "UART READY"。
  *
  * 模块划分：
  *   OLED/oled.c                 SSD1306 驱动（8x16 字符 + 16x16 汉字）
  *   SOFT_I2C/soft_i2c.c         软件模拟 I²C
  *   JSON_PARSER/json_parser.c   从 JSON 中取字段、去 "Good" 前缀
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
#include "oled.h"
#include "json_parser.h"
#include "uart_rx.h"
#include <string.h>
#include <stdint.h>
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

/* 串口单字节接收缓冲（交给 HAL 使用） */
uint8_t rx_byte;

/* 队列由 CubeMX 在 freertos.c 里创建
   （FREERTOS -> Tasks and Queues -> Queues） */
extern osMessageQueueId_t uartRxQueueHandle;   /* 中断收字节 -> TaskUartRx */

/* 裸机版那套 uart_rx_buf / rx_idx / rx_saw_brace / frame_buffer / frame_ready
   全部删掉了：
     - 攒帧逻辑搬进了 UART_RX/uart_rx.c（由 TaskUartRx 调用）
     - "中断发布整帧、主循环读取"的双缓冲被 frameQueue 取代
   原来的单缓冲有个真实缺陷：刷屏要 60ms，这期间到达的新帧会直接覆盖掉
   上一帧。现在帧进队列，队列满时明确丢最旧的一帧（见 freertos.c 的
   TaskUartRx），主机最后发的那条指令不会丢。 */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
void App_ProcessFrame(const char *line);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  处理一帧分拣指令：解析四个方向 -> 去 Good 前缀 -> 刷新 OLED
  * @param  line 一整帧 JSON，由 TaskDisplay 从 frameQueue 取出后传入
  * @note   由 TaskDisplay 调用（最低优先级，刷一屏约 60ms）。
  *
  *         裸机版这里要先关中断把 frame_buffer 整个拷进 local_buf 再解析；
  *         现在字节走队列、帧也走队列，传进来的本来就是任务私有的一份副本，
  *         那 256 字节的拷贝和临界区可以整个删掉。
  */
void App_ProcessFrame(const char *line)
{
    char down[30]  = {0};//作用是存储从JSON中解析出来的"Down"方向的货物名
    char up[30]    = {0};//作用是存储从JSON中解析出来的"Up"方向的货物名
    char left[30]  = {0};//作用是存储从JSON中解析出来的"Left"方向的货物名
    char right[30] = {0};//作用是存储从JSON中解析出来的"Right"方向的货物名

    Json_GetValue(line, "Down",  down,  sizeof(down));
    Json_GetValue(line, "Up",    up,    sizeof(up));
    Json_GetValue(line, "Left",  left,  sizeof(left));
    Json_GetValue(line, "Right", right, sizeof(right));

    /* 去掉货物名前的 "Good" 前缀，例如 "GoodA,GoodB" -> "A,B" */
    Json_StripGoodPrefix(down,  sizeof(down));
    Json_StripGoodPrefix(up,    sizeof(up));
    Json_StripGoodPrefix(left,  sizeof(left));
    Json_StripGoodPrefix(right, sizeof(right));

    /* 四行显示：方向用 16x16 汉字，货物名用 8x16 字符 */
    OLED_Clear();
    OLED_ShowChinese(0, 0, 0);      /* 上 */
    OLED_ShowString(20, 0, up);
    OLED_ShowChinese(0, 2, 1);      /* 下 */
    OLED_ShowString(20, 2, down);
    OLED_ShowChinese(0, 4, 2);      /* 左 */
    OLED_ShowString(20, 4, left);
    OLED_ShowChinese(0, 6, 3);      /* 右 */
    OLED_ShowString(20, 6, right);
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
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
    SoftI2C_Init();
    OLED_Init();
    OLED_Clear();
    OLED_ShowMiniStr(0, 2, "WAIT DATA");

    /* 调试口打招呼。长度用 strlen 取，不手写数字 */
    {
        static const char msg_ready[] = "UART READY\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t *)msg_ready, (uint16_t)strlen(msg_ready), 50);
    }

    /* 串口接收不在这里使能！
       要等到 uartRxQueue 建好之后（见 freertos.c 的 USER CODE BEGIN RTOS_THREADS）。
       在这里调 HAL_UART_Receive_IT 的话，从中断使能到 MX_FREERTOS_Init() 建好队列
       之间有窗口，这期间到达的字节会因为句柄还是 NULL 被静默丢掉。 */
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
       原先的主循环（查 frame_ready -> ProcessFrame -> HAL_Delay(50)）
       已经拆成 freertos.c 里的两个任务：
         TaskUartRx —— 从 uartRxQueue 取字节，攒成整帧丢进 frameQueue
         TaskDisplay —— 从 frameQueue 取帧，解析并刷 OLED */
    while (1)
    {
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
  *
  *         原来在这里干的重活（逐字节攒进 uart_rx_buf、判 '}' 和 '\n'、
  *         memcpy 整帧到 frame_buffer）全部搬进了 UART_RX/uart_rx.c，
  *         由 TaskUartRx 调用 —— 中断越短越好。
  *
  *         osMessageQueuePut 是 ISR 安全的（靠 __get_IPSR() 自动分辨），
  *         但在中断里超时参数必须是 0。
  *
  *         ⚠ 本函数在 USART1 中断里跑，它的抢占优先级必须是 5
  *         （configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY），
  *         见 usart.c 里的 HAL_NVIC_SetPriority。优先级比 5 高（数值更小）
  *         的 ISR 调 FreeRTOS API 是非法的。
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        (void)osMessageQueuePut(uartRxQueueHandle, &rx_byte, 0U, 0U);

        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);   /* 重新使能接收 */
    }
}

/**
  * @brief  串口错误回调（溢出 ORE / 帧错 / 噪声 / 校验错）
  * @note   必须实现它，否则一次接收溢出就会让 USART1【永久失聪】。
  *
  *         为什么会上电就溢出：
  *           MX_USART1_UART_Init() 里 Mode = UART_MODE_TX_RX，
  *           接收器从那一刻起就在工作，但 RXNEIE 要等到 freertos.c 里的
  *           HAL_UART_Receive_IT() 才打开 —— 中间隔着 OLED_Init() 的
  *           HAL_Delay(100) 和两次清屏，约 250ms。
  *           这期间到达的字节没人读 DR：第 1 个置 RXNE，第 2 个起 ORE 置位。
  *
  *         而 HAL 把 ORE 当"阻塞性错误"处理（stm32f1xx_hal_uart.c:2411），
  *         中断里的顺序是：
  *           ① UART_Receive_IT() 读走 DR（清掉 ORE），回调里我们重新武装 RXNEIE
  *           ② 紧接着 UART_EndRxTransfer() 又把 RXNEIE 关掉
  *         结果是 RXNEIE 恒为 0，串口静默死亡直到复位 ——
  *         调度器、显示都正常，只是再也收不到一个字节，极难定位。
  *
  *         这里把接收重新武装起来，把"永久失聪"降级成"丢一帧"。
  *         HAL 在回调返回后会把 huart->ErrorCode 清掉，不用我们管。
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        /* UART_EndRxTransfer() 已把 RxState 置回 READY，所以这里能重新武装。
           万一返回 HAL_BUSY（说明别的路径已经武装过了），忽略即可。 */
        (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
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
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

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
char buffer1[32];            /* 串口收到的原始行（接收中断写入） */
uint8_t rx_idx = 0;          /* buffer1 写入位置（仅接收中断访问） */
volatile uint8_t rflag1 = 0; /* 收到完整一行的标志：中断置 1、主循环清 0 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
static void Counter_Set(int value);
static void process_usart1_data(void);
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
static void Counter_Set(int value) {
  if (value < 0)
    value = 0;
  if (value > 99)
    value = 99;
  GoodsNumber = (uint8_t)value;
}

/**
 * @brief  解析串口下行指令 {"GoodsNumber":"x"}\r\n
 * @note   buffer1/rflag1 由接收中断写入。这里先在关中断状态下把数据
 *         整体拷到本地缓冲再解析，避免解析途中被新一帧数据改写。
 */
static void process_usart1_data(void) {
  char local_buf[sizeof(
      buffer1)]; // 作用是创建一个本地缓冲区 local_buf，其大小与 buffer1
                 // 相同，用于在关中断状态下临时存储接收到的串口数据，避免在解析过程中被新的数据覆盖。
  char num_str[10]; // 作用是创建一个字符数组 num_str，用于存储从 JSON
                    // 字符串中提取的计数值字符串，大小为 10
                    // 个字符，足够容纳计数值及其引号。
  uint32_t
      primask; // 作用是定义一个无符号 32 位整数变量
               // primask，用于保存当前中断状态，以便在关中断后恢复原有中断状态。

  if (rflag1 == 0)
    return; // 作用是检查 rflag1 标志位是否为 0，如果为
            // 0，表示没有完整的数据帧可供处理，直接返回函数，不进行后续解析操作。

  /* ---- 关中断，原子地取走一帧数据并复位接收状态 ---- */
  primask = __get_PRIMASK(); // 作用是调用 __get_PRIMASK()
                             // 函数获取当前中断状态，并将其保存到 primask
                             // 变量中，以便在处理完数据后恢复原有中断状态。
  __disable_irq(); // 作用是调用 __disable_irq()
                   // 函数禁用全局中断，确保在接下来的操作中不会被中断打断，从而保证数据的一致性和完整性。

  memcpy(local_buf, buffer1,
         sizeof(buffer1)); // 作用是将接收到的串口数据从全局缓冲区 buffer1
                           // 复制到本地缓冲区 local_buf
                           // 中，确保在解析过程中不会被新的数据覆盖。
  local_buf[sizeof(local_buf) - 1] =
      '\0'; // 作用是确保 local_buf 字符串以空字符 '\0'
            // 结尾，防止在后续字符串操作中出现越界访问或未定义行为。

  rflag1 = 0; // 作用是将 rflag1 标志位复位为
              // 0，表示已经处理完当前数据帧，可以接收新的数据帧。
  rx_idx = 0; // 作用是将接收索引 rx_idx 复位为 0，准备接收新的数据帧。
  memset(buffer1, 0,
         sizeof(buffer1)); // 作用是将全局缓冲区 buffer1
                           // 清零，确保在接收新的数据帧时不会受到旧数据的干扰。

  __set_PRIMASK(primask);
  /* ---- 临界区结束 ---- */

  if (sscanf(local_buf, "{\"GoodsNumber\":\"%[^\"]\"}", num_str) ==
      1) // 作用是使用 sscanf 函数从 local_buf 中解析 JSON 字符串，提取
         // "GoodsNumber" 对应的值，并将其存储在 num_str
         // 中。如果解析成功（返回值为 1），则表示成功提取到计数值字符串。
  {
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

  /* 启动串口单字节中断接收 */
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

  /* 上电立即显示当前计数值 */
  Display_ShowNum(GoodsNumber);
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* ---- 1. 串口下行指令（放在按键之前，保持原有执行顺序） ---- */
    process_usart1_data();

    /* ---- 2. 按键：短按 / 长按连发 ----
     * 只有计数值确实改变时才上报，两个原因：
     *   a) 到 99（或减到 0）后继续长按，不会向串口持续刷屏；
     *   b) 上报次数与设备实际计数一一对应，上位机的镜像计数不会跑偏。
     */
    switch (Key_Process()) {
    case KEY_EVENT_ADD:
      if (GoodsNumber < 99) {
        Counter_Set(GoodsNumber + 1);
        Uart_EventPush(UART_EVT_ADD);
      }
      break;

    case KEY_EVENT_SUB:
      if (GoodsNumber > 0) {
        Counter_Set(GoodsNumber - 1);
        Uart_EventPush(UART_EVT_SUB);
      }
      break;

    case KEY_EVENT_CLEAR:
      /* 清零是幂等的（上位机置 0 不会产生偏差），直接上报 */
      Counter_Set(0);
      Uart_EventPush(UART_EVT_ZERO);
      break;

    default:
      break;
    }

    /* ---- 3. 串口事件上报（中断只入队，这里才真正发送） ---- */
    Uart_EventProcess();

    /* ---- 4. 数码管动态扫描：必须每轮调用，否则只有一位亮 ---- */
    Display_ShowNum(GoodsNumber);
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
 * @note   逐字节累积到 buffer1，遇到 \r 或 \n 认为一帧结束，置位 rflag1。
 *         本函数不做解析，解析放在主循环的 process_usart1_data() 中。
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) // 作用是判断当前中断回调是由哪个串口触发的，这里是判断是否为
                                 // USART1 触发的接收完成中断。
  {
    if (rx_idx < sizeof(buffer1) - 1) // 作用：检查接收索引是否在缓冲区范围内
    {
      buffer1[rx_idx++] =
          (char)rx_byte; // 作用：将接收到的字节存入缓冲区，并递增索引
      buffer1[rx_idx] = '\0'; /* 保证始终是合法字符串 */
    }

    if (rx_byte == '\r' ||
        rx_byte == '\n') // 作用是判断接收到的字节是否为回车或换行字符，如果是，则认为一帧数据接收完成，设置
                         // rflag1 标志位为 1，表示有完整的数据帧可供处理。
    {
      rflag1 = 1;
    }

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

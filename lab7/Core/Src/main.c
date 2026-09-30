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
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "key.h"
#include "display.h"
#include "uart_send.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t  GoodsNumber = 0;         /* 当前计数值 0~99 */

uint8_t  rx_byte;                 /* 串口单字节接收缓冲（交给 HAL 使用） */
char     buffer1[32];             /* 串口收到的原始行（接收中断写入） */
uint8_t  rx_idx = 0;              /* buffer1 写入位置（仅接收中断访问） */
volatile uint8_t rflag1 = 0;      /* 收到完整一行的标志：中断置 1、主循环清 0 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void Counter_Set(int value);
static void process_usart1_data(void);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/**
  * @brief  统一设置计数值
  * @param  value 目标值，越界时钳位到 0~99
  * @note   按键与串口下行都经过本函数，保证钳位规则只有一处。
  *         注意入参是 int：atoi() 返回的可能超过 99，
  *         若先转成 uint8_t 再判断会发生回绕（例如 300 -> 44）。
  */
static void Counter_Set(int value)
{
    if (value < 0)    value = 0;
    if (value > 99)   value = 99;
    GoodsNumber = (uint8_t)value;
}

/**
  * @brief  解析串口下行指令 {"GoodsNumber":"x"}\r\n
  * @note   buffer1/rflag1 由接收中断写入。这里先在关中断状态下把数据
  *         整体拷到本地缓冲再解析，避免解析途中被新一帧数据改写。
  */
static void process_usart1_data(void)
{
    char     local_buf[sizeof(buffer1)];
    char     num_str[10];
    uint32_t primask;

    if (rflag1 == 0) return;

    /* ---- 关中断，原子地取走一帧数据并复位接收状态 ---- */
    primask = __get_PRIMASK();
    __disable_irq();

    memcpy(local_buf, buffer1, sizeof(buffer1));
    local_buf[sizeof(local_buf) - 1] = '\0';

    rflag1 = 0;
    rx_idx = 0;
    memset(buffer1, 0, sizeof(buffer1));

    __set_PRIMASK(primask);
    /* ---- 临界区结束 ---- */

    if (sscanf(local_buf, "{\"GoodsNumber\":\"%[^\"]\"}", num_str) == 1)
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
  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

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

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* ---- 1. 串口下行指令（放在按键之前，保持原有执行顺序） ---- */
    process_usart1_data();

    /* ---- 2. 按键：短按 / 长按连发 ----
     * 只有计数值确实改变时才上报，两个原因：
     *   a) 到 99（或减到 0）后继续长按，不会向串口持续刷屏；
     *   b) 上报次数与设备实际计数一一对应，上位机的镜像计数不会跑偏。
     */
    switch (Key_Process())
    {
        case KEY_EVENT_ADD:
            if (GoodsNumber < 99)
            {
                Counter_Set(GoodsNumber + 1);
                Uart_EventPush(UART_EVT_ADD);
            }
            break;

        case KEY_EVENT_SUB:
            if (GoodsNumber > 0)
            {
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
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (rx_idx < sizeof(buffer1) - 1)
        {
            buffer1[rx_idx++] = (char)rx_byte;
            buffer1[rx_idx]   = '\0';       /* 保证始终是合法字符串 */
        }

        if (rx_byte == '\r' || rx_byte == '\n')
        {
            rflag1 = 1;
        }

        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);   /* 重新使能接收 */
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
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

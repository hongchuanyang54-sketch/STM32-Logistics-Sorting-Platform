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
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "oled.h"
#include "json_parser.h"
#include <string.h>
#include <stdint.h>
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/

/* USER CODE BEGIN PD */
/* 单帧最大长度，同时也是帧缓冲（双缓冲）的大小 */
#define RX_BUFFER_MAX   256
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* ---- USART1 接收（中断写入） ---- */
static uint8_t          rx_byte;                          /* 单字节接收缓冲 */
static char             uart_rx_buf[RX_BUFFER_MAX];       /* 攒帧用，中断独占 */
static uint16_t         rx_idx = 0;                       /* uart_rx_buf 写入位置 */
static uint8_t          rx_saw_brace = 0;                 /* 本帧是否已见到 '}' */

/* ---- 双缓冲：中断把完整帧发布到这里，主循环读取 ---- */
static char             frame_buffer[RX_BUFFER_MAX];
static volatile uint8_t frame_ready = 0;                  /* 中断置 1，主循环处理时清 0 */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void ProcessFrame(void);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/**
  * @brief  处理一帧分拣指令：解析四个方向 -> 去 Good 前缀 -> 刷新 OLED
  * @note   frame_buffer 由接收中断写入，这里先在关中断状态下整体拷贝再解析。
  */
static void ProcessFrame(void)
{
    char     local_buf[RX_BUFFER_MAX];
    char     down[30]  = {0};
    char     up[30]    = {0};
    char     left[30]  = {0};
    char     right[30] = {0};
    uint32_t primask;

    /* ---- 临界区：原子取走一帧 ---- */
    primask = __get_PRIMASK();
    __disable_irq();
    memcpy(local_buf, frame_buffer, sizeof(frame_buffer));
    local_buf[sizeof(local_buf) - 1] = '\0';
    __set_PRIMASK(primask);
    /* ---- 临界区结束 ---- */

    Json_GetValue(local_buf, "Down",  down,  sizeof(down));
    Json_GetValue(local_buf, "Up",    up,    sizeof(up));
    Json_GetValue(local_buf, "Left",  left,  sizeof(left));
    Json_GetValue(local_buf, "Right", right, sizeof(right));

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
    /* MCU Configuration--------------------------------------------------------*/
    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

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

    /* 启动串口单字节中断接收 */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        if (frame_ready)
        {
            frame_ready = 0;    /* 先清标志：拷贝期间到达的新帧会在下一轮再处理 */
            ProcessFrame();
        }

        HAL_Delay(50);
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

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
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
  * @note   帧完整性判定采用 '}' + '\n' 双标记：
  *           - 逐字节攒进 uart_rx_buf，同时记录是否见过 '}'；
  *           - 收到 '\n' 时，只有见过 '}' 才把整帧发布到 frame_buffer；
  *           - '\r' 直接跳过，兼容 CRLF 与单独 LF 两种行尾；
  *           - 缓冲写满仍未成帧则整帧丢弃，避免缓冲区溢出。
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (rx_byte == '\n')
        {
            if (rx_idx > 0 && rx_saw_brace)
            {
                uart_rx_buf[rx_idx] = '\0';
                memcpy(frame_buffer, uart_rx_buf, rx_idx + 1);
                frame_ready = 1;
            }
            rx_idx        = 0;
            rx_saw_brace  = 0;
        }
        else if (rx_byte != '\r')
        {
            if (rx_idx < RX_BUFFER_MAX - 1)
            {
                uart_rx_buf[rx_idx++] = (char)rx_byte;
                if (rx_byte == '}')
                {
                    rx_saw_brace = 1;
                }
            }
            else
            {
                rx_idx       = 0;       /* 过长帧丢弃，重新开始攒 */
                rx_saw_brace = 0;
            }
        }

        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);   /* 重新使能接收 */
    }
}

/* USER CODE END 4 */

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif

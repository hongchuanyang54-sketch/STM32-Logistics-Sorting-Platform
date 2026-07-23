/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "json_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RX_BUFFER_MAX 256
#define USART2_RX_MAX 64
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

uint8_t uart_rx_buf[RX_BUFFER_MAX] = {0};
uint16_t rx_index = 0;
uint8_t frame_finish_flag = 0;
char    frame_buffer[RX_BUFFER_MAX] = {0};  /* 双缓冲: ISR拷贝完成帧,主循环读取 */

uint8_t uart2_rx_buf[USART2_RX_MAX] = {0};
uint16_t uart2_rx_idx = 0;
uint8_t uart2_frame_flag = 0;
char cmd_buf[32];
uint8_t uart2_temp_ch;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void get_json_value(char *json, char *key, char *out);
void remove_good_prefix(char *src);
void process_usart1_data(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// 简单JSON解析：从json字符串中提取key对应的值
void get_json_value(char *json, char *key, char *out)
{
    char *p = strstr(json, key);
    if (p == NULL) {
        out[0] = '\0';
        return;
    }
    char *start = strchr(p, ':');
    if (start == NULL) {
        out[0] = '\0';
        return;
    }
    start = strchr(start, '"');
    if (start == NULL) {
        out[0] = '\0';
        return;
    }
    start++;
    char *end = strchr(start, '"');
    if (end == NULL) {
        out[0] = '\0';
        return;
    }
    strncpy(out, start, end - start);
    out[end - start] = '\0';
    out[29] = '\0'; // 截断保护
}

/**
 * 去掉货物名称前面的 "good"/"Good"/"GOOD" 前缀(不区分大小写)
 * 例如: "GoodA,GoodB" -> "A,B"
 * 注意: 不使用 strtok,避免破坏原字符串
 */
void remove_good_prefix(char *src)
{
    char temp_buf[30] = {0};
    char result[30] = {0};
    strcpy(temp_buf, src);

    char *token_start = temp_buf;
    int first = 1;

    while (*token_start != '\0')
    {
        // 跳过前导空格
        while (*token_start == ' ') token_start++;
        if (*token_start == '\0') break;

        // 找到当前 token 的结尾(逗号或字符串结束)
        char *token_end = strchr(token_start, ',');
        char saved_char = 0;
        if (token_end != NULL)
        {
            saved_char = *token_end;
            *token_end = '\0';  // 临时截断当前token
        }

        // 非首个token加逗号分隔
        if (!first)
            strcat(result, ",");

        // 转小写后比较
        char low_buf[20] = {0};
        strcpy(low_buf, token_start);
        for (int i = 0; low_buf[i] != '\0'; i++)
            low_buf[i] = tolower(low_buf[i]);

        // 匹配 "good" 前缀,去掉前4个字符
        if (strncmp(low_buf, "good", 4) == 0)
            strcat(result, token_start + 4);
        else
            strcat(result, token_start);

        // 恢复逗号,继续下一个token
        if (token_end != NULL)
        {
            *token_end = saved_char;
            token_start = token_end + 1;
        }
        else
        {
            break;  // 最后一个token
        }
        first = 0;
    }

    strcpy(src, result);
}

// 处理接收到的完整JSON帧(从双缓冲frame_buffer读取)
void process_usart1_data(void)
{
    char local_buf[RX_BUFFER_MAX];
    char down[30] = {0};
    char up[30] = {0};
    char left[30] = {0};
    char right[30] = {0};

    // 原子拷贝: 禁用IRQ防止ISR同时写frame_buffer导致数据损坏
    __disable_irq();
    strncpy(local_buf, frame_buffer, RX_BUFFER_MAX - 1);
    local_buf[RX_BUFFER_MAX - 1] = '\0';
    __enable_irq();

    get_json_value(local_buf, "Down", down);
    get_json_value(local_buf, "Up", up);
    get_json_value(local_buf, "Left", left);
    get_json_value(local_buf, "Right", right);

    // 去掉 good 前缀(如果存在的话)
    remove_good_prefix(down);
    remove_good_prefix(up);
    remove_good_prefix(left);
    remove_good_prefix(right);

    // 更新OLED显示
    OLED_Clear();
    OLED_ShowChinese(0, 0, 0);  // 上
    OLED_ShowString(20, 0, up);
    OLED_ShowChinese(0, 2, 1);  // 下
    OLED_ShowString(20, 2, down);
    OLED_ShowChinese(0, 4, 2);  // 左
    OLED_ShowString(20, 4, left);
    OLED_ShowChinese(0, 6, 3);  // 右
    OLED_ShowString(20, 6, right);
}

// UART接收完成回调
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    // USART1: 收到 '}' 表示一帧结束
    if (huart->Instance == USART1)
    {
        uint8_t ch = uart_rx_buf[rx_index];
        if (ch == '}')
        {
            // 帧尾: 确保frame_buffer里有完整JSON
            // 在 '}' 后面补 '\0',继续接收 \r\n 但不影响解析
            rx_index++;
            if (rx_index >= RX_BUFFER_MAX - 1)
            {
                memset(uart_rx_buf, 0, RX_BUFFER_MAX);
                rx_index = 0;
                frame_finish_flag = 0;
            }
            // 等待 \n 确认帧结束(下一个字节)
            HAL_UART_Receive_IT(&huart1, &uart_rx_buf[rx_index], 1);
        }
        else if (ch == '\n')
        {
            // \n: 帧真正结束,截断JSON(去掉 \r\n)
            uart_rx_buf[rx_index - 2] = '\0';  // 在 } 后面截断

            // 拷贝到双缓冲,主循环稍后读取
            strncpy(frame_buffer, (const char *)uart_rx_buf, RX_BUFFER_MAX - 1);
            frame_buffer[RX_BUFFER_MAX - 1] = '\0';

            frame_finish_flag = 1;
            rx_index = 0;

            // 立即重新启用接收,不等待主循环 — OLED操作期间不会丢数据
            HAL_UART_Receive_IT(&huart1, &uart_rx_buf[0], 1);
        }
        else
        {
            rx_index++;
            if (rx_index >= RX_BUFFER_MAX - 1)
            {
                memset(uart_rx_buf, 0, RX_BUFFER_MAX);
                rx_index = 0;
                frame_finish_flag = 0;
            }
            HAL_UART_Receive_IT(&huart1, &uart_rx_buf[rx_index], 1);
        }
    }

    // USART2: 用 '\r' 或 '\n' 判断帧结束
    if (huart->Instance == USART2)
    {
        uint8_t ch = uart2_temp_ch;
        if (ch == '\r' || ch == '\n')
        {
            if (uart2_rx_idx > 0)
            {
                uart2_rx_buf[uart2_rx_idx] = '\0';
                uart2_frame_flag = 1;
            }
            uart2_rx_idx = 0;
        }
        else
        {
            if (uart2_rx_idx < USART2_RX_MAX - 1)
            {
                uart2_rx_buf[uart2_rx_idx++] = ch;
            }
            else
            {
                memset(uart2_rx_buf, 0, USART2_RX_MAX);
                uart2_rx_idx = 0;
            }
        }
        HAL_UART_Receive_IT(&huart2, &uart2_temp_ch, 1);
    }
}
/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    /* USER CODE BEGIN 2 */
    SoftI2C_Init();
    OLED_Init();
    OLED_Clear();
    OLED_ShowMiniStr(0, 2, "WAIT DATA");

    char test_msg[] = "UART READY\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t *)test_msg, strlen(test_msg), 50);

    // 启动中断接收
    HAL_UART_Receive_IT(&huart1, &uart_rx_buf[rx_index], 1);
    HAL_UART_Receive_IT(&huart2, &uart2_temp_ch, 1);
    /* USER CODE END 2 */

    /* Infinite loop */
    while (1)
    {
        // 检查 USART1 完整帧(frame_buffer中已有完整JSON)
        if (frame_finish_flag == 1)
        {
            frame_finish_flag = 0;

            // 处理帧数据:解析JSON+更新显示
            // 注意: ISR在拷贝到frame_buffer后已立即重新启用接收,
            //       所以OLED操作期间不会丢失新数据
            process_usart1_data();
        }

        HAL_Delay(50);
    }
    /* USER CODE END 3 */
}

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

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @brief ???? ???? ???? ????JSON:{GoodsNumber}:x
  ******************************************************************************/
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "key.h"
#include "display.h"
#include "uart_send.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
uint8_t GoodsNumber = 0;
volatile uint8_t uart_send_flag = 0;
uint8_t rx_byte;
char buffer1[32];
uint8_t rx_idx = 0;
uint8_t rflag1 = 0;

uint8_t key_press_flag = 0;
uint32_t key_press_tick = 0;
uint32_t last_scan_tick = 0;
#define KEY_SHORT_DELAY 200
#define KEY_LONG_SPEED 200
uint32_t key_long_tick;
uint8_t key_single_flag = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void process_usart1_data(void);
void Display_ShowNum(uint8_t num);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  ??????????,??????????
  *         ????: {"GoodsNumber":"x"}\r\n (x???????)
  * @retval None
  */
void process_usart1_data(void)
{
    /* ??????????,????? */
    if(rflag1 == 0)
        return;

    char num_str[10];

    /* ?sscanf???????,????????????? */
    if(sscanf((char*)buffer1, "{\"GoodsNumber\":\"%[^\"]\"}", num_str) == 1)
    {
        /* ????????????,???????GoodsNumber */
        GoodsNumber = (uint8_t)atoi(num_str);

        /* ?????0~99 */
        if(GoodsNumber > 99)
            GoodsNumber = 99;

        /* ?????????? */
        Display_ShowNum(GoodsNumber);
    }

    /* ??????????,???????? */
    rflag1 = 0;
    rx_idx = 0;
    memset(buffer1, 0, sizeof(buffer1));
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
  Key_Init();
  Display_Init();
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // ??????,??????
    if(key_press_flag == 1 && HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY_ADD_PIN) == GPIO_PIN_RESET)
    {
        key_press_flag = 0;
        key_single_flag = 0;
    }
    if(key_press_flag == 2 && HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY_SUB_PIN) == GPIO_PIN_RESET)
    {
        key_press_flag = 0;
        key_single_flag = 0;
    }
    if(key_press_flag == 3 && HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY_RST_PIN) == GPIO_PIN_RESET)
    {
        key_press_flag = 0;
        key_single_flag = 0;
    }

    process_usart1_data();

    if(key_press_flag != 0)
    {
        uint32_t now = HAL_GetTick();
        // ??200ms????????
        if(now - key_press_tick < KEY_SHORT_DELAY)
        {
            if(key_single_flag == 0)
            {
                switch(key_press_flag)
                {
                    case 1:
                        if(GoodsNumber < 99)
                        {
                            GoodsNumber++;
                            /* Send_Add() ????EXTI?????????,?????? */
                        }
                        break;
                    case 2:
                        if(GoodsNumber > 0)
                        {
                            GoodsNumber--;
                            /* Send_Sub() ????EXTI?????????,?????? */
                        }
                        break;
                    case 3:
                        GoodsNumber = 0;
                        /* Send_Zero() ????EXTI?????????,?????? */
                        key_press_flag = 0;
                        key_single_flag = 0;
                        break;
                }
                key_single_flag = 1;
                key_long_tick = now;
            }
        }
        // ??????
        else if(now - key_long_tick >= KEY_LONG_SPEED)
        {
            switch(key_press_flag)
            {
                case 1:
                    if(GoodsNumber < 99)
                    {
                        GoodsNumber++;
                        Send_Add();
                    }
                    break;
                case 2:
                    if(GoodsNumber > 0)
                    {
                        GoodsNumber--;
                        Send_Sub();
                    }
                    break;
                default: break;
            }
            key_long_tick = now;
        }
    }

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
// ????????
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    static uint32_t last_tick[3] = {0,0,0};
    uint32_t current_time = HAL_GetTick();
    uint8_t idx = 255;

    /* KEY4 (PA0) ????: ??????????,?????? */
    if(GPIO_Pin == KEY4_Pin)
    {
        static uint32_t last_estop_tick = 0;
        if(current_time - last_estop_tick < KEY_SHORT_DELAY)
            return;
        last_estop_tick = current_time;
        Send_EStop();
        return;
    }

    /* KEY_RE (PA6) ????: ??????????,?????? */
    if(GPIO_Pin == KEY_RE_Pin)
    {
        static uint32_t last_reset_tick = 0;
        if(current_time - last_reset_tick < KEY_SHORT_DELAY)
            return;
        last_reset_tick = current_time;
        Send_Reset();
        return;
    }

    /* ?????? */
    if(GPIO_Pin == KEY1_Pin)      idx = 0;
    else if(GPIO_Pin == KEY2_Pin) idx = 1;
    else if(GPIO_Pin == KEY3_Pin) idx = 2;
    if(idx == 255) return;

    /* ???? (??200ms) */
    if(current_time - last_tick[idx] < KEY_SHORT_DELAY)
        return;
    last_tick[idx] = current_time;

    /* ????????? */
    key_press_flag = idx + 1;
    key_press_tick = current_time;

    /* ?????????????????? */
    switch(idx)
    {
        case 0:  Send_Add();   break;  /* KEY1: ?? --> ?? "ADD\r\n"   */
        case 1:  Send_Sub();   break;  /* KEY2: ?? --> ?? "SUB\r\n"   */
        case 2:  Send_Zero();  break;  /* KEY3: ?? --> ?? "ZERO\r\n"  */
        default: break;
    }
}

// ??????(?????,?????)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1)
    {
        if(rx_idx < sizeof(buffer1)-1)
        {
            buffer1[rx_idx++] = rx_byte;
            buffer1[rx_idx] = '\0';
        }
        // ??\r?\n??????
        if(rx_byte == '\r' || rx_byte == '\n')
        {
            rflag1 = 1;
        }
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
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

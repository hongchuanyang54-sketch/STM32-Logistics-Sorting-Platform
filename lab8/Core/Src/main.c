/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   传送带控制器（lab8）
  *
  * 功能：
  *   1. 电位器 → ADC → 10 档调速（ADC 模式）
  *   2. 串口 JSON {"Speed":"x"}\r\n 远程调速（UART 模式）
  *   3. 串口文本命令 F<n> / R<n> / STOP / ADC，用于调试
  *   4. OLED 实时显示 模式 / 方向 / 档位 / 占空比 / ADC 值
  *   5. 每 100ms 上行上报 Speed:<档位>\r\n
  *
  * 两种控制模式的仲裁：谁最后发指令就听谁的。
  *   收到串口指令 → 切换到 MODE_UART，ADC 不再干预转速；
  *   收到 "ADC"   → 切回 MODE_ADC，重新由电位器控制。
  *
  * 主循环节拍： ADC 25ms / 电机 15ms / 显示与上报 100ms
  *
  * 模块划分：
  *   MOTOR/motor.c        电机状态机（平滑变速、安全换向、PWM 启停）
  *   OLED/oled.c          SSD1306 显示驱动
  *   SOFT_I2C/soft_i2c.c  软件模拟 I²C
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "oled.h"
#include "motor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/

/* USER CODE BEGIN PD */
/* 控制模式 */
typedef enum
{
    MODE_ADC  = 0,      /* 由电位器 ADC 值决定转速 */
    MODE_UART = 1       /* 由串口指令决定转速   */
} CtrlMode_t;

/* 主循环各任务节拍（ms） */
#define ADC_PERIOD_MS   25
#define UI_PERIOD_MS    100
/* 电机节拍直接用 MOTOR_TICK_MS（15ms），见 MOTOR/motor.h */
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static CtrlMode_t ctrl_mode = MODE_ADC;
static uint32_t   adc_value = 0;

/* 串口接收：中断逐字节累积，收到 \r 或 \n 认为一帧结束 */
static uint8_t          rx_byte;
static char             rx_buf[50];          /* 最长一帧 49 字符，够放 JSON */
static uint8_t          rx_idx = 0;
static volatile uint8_t rx_frame_ready = 0;  /* 中断置 1，主循环处理时清 0 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void process_usart1_data(void);
static void HandleLegacyCommand(char *cmd);
static void SendToUSART1(void);
static void OLED_Refresh(void);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/**
  * @brief  解析一帧串口数据
  * @note   先按 JSON 调速指令解析，不匹配再按旧文本命令处理。
  *         rx_buf / rx_frame_ready 由接收中断写入，这里先在关中断状态下
  *         整体拷贝再解析，避免解析途中被下一帧改写。
  */
static void process_usart1_data(void)
{
    char     local_buf[sizeof(rx_buf)];
    char     speed_str[10];
    uint32_t primask;
    int      speed;

    if (rx_frame_ready == 0) return;

    /* ---- 临界区：原子取走一帧并复位接收状态 ---- */
    primask = __get_PRIMASK();
    __disable_irq();

    memcpy(local_buf, rx_buf, sizeof(rx_buf));
    local_buf[sizeof(local_buf) - 1] = '\0';

    rx_frame_ready = 0;
    rx_idx         = 0;
    memset(rx_buf, 0, sizeof(rx_buf));

    __set_PRIMASK(primask);
    /* ---- 临界区结束 ---- */

    /* ---- 1. JSON 远程调速：{"Speed":"x"} ---- */
    if (sscanf(local_buf, "{\"Speed\":\"%[^\"]\"}", speed_str) == 1)
    {
        speed = atoi(speed_str);

        if (speed >= 0 && speed <= MOTOR_GEAR_MAX)
        {
            ctrl_mode = MODE_UART;      /* 收到远程指令后锁定串口模式，防止 ADC 覆盖 */

            if (speed == 0)
            {
                Motor_SetTarget(MOTOR_STOP, 0);
            }
            else
            {
                Motor_SetTarget(MOTOR_FWD, (uint8_t)speed);   /* 默认正转 */
            }
        }
        return;
    }

    /* ---- 2. 旧文本命令 ---- */
    HandleLegacyCommand(local_buf);
}

/* ---- 固定应答报文。长度一律用 strlen 取，不手写数字，避免数错 ---- */
static const char MSG_ERR_GEAR[] = "Error: Gear 1-10\r\n";
static const char MSG_OK_STOP[]  = "OK: STOP\r\n";
static const char MSG_OK_ADC[]   = "OK: ADC Mode\r\n";
static const char MSG_UNKNOWN[]  = "Unknown Command\r\n";

/**
  * @brief  发送一段文本（阻塞发送，最长超时 100ms）
  */
static void Uart_SendText(const char *text)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 100);
}

/**
  * @brief  处理旧文本命令：F<n> / R<n> / STOP / ADC
  * @note   第一段是防回环过滤：本机自己发出的上报和应答会经 TX 串回 RX，
  *         不滤掉会造成死循环。
  */
static void HandleLegacyCommand(char *cmd)
{
    char res[32];
    int  g;

    /* ---- 防回环 ---- */
    if (strncmp(cmd, "Speed:",  6) == 0 ||
        strncmp(cmd, "OK:",     3) == 0 ||
        strncmp(cmd, "Error:",  6) == 0 ||
        strncmp(cmd, "Unknown", 7) == 0 ||
        cmd[0] == '{')
    {
        return;
    }

    /* ---- F<n>：正转 n 档 ---- */
    if (cmd[0] == 'F' || cmd[0] == 'f')
    {
        g = atoi(cmd + 1);
        if (g >= 1 && g <= MOTOR_GEAR_MAX)
        {
            ctrl_mode = MODE_UART;
            Motor_SetTarget(MOTOR_FWD, (uint8_t)g);
            sprintf(res, "OK: FWD %d Gear\r\n", g);
            Uart_SendText(res);
        }
        else
        {
            Uart_SendText(MSG_ERR_GEAR);
        }
    }
    /* ---- R<n>：反转 n 档 ---- */
    else if (cmd[0] == 'R' || cmd[0] == 'r')
    {
        g = atoi(cmd + 1);
        if (g >= 1 && g <= MOTOR_GEAR_MAX)
        {
            ctrl_mode = MODE_UART;
            Motor_SetTarget(MOTOR_REV, (uint8_t)g);
            sprintf(res, "OK: REV %d Gear\r\n", g);
            Uart_SendText(res);
        }
        else
        {
            Uart_SendText(MSG_ERR_GEAR);
        }
    }
    /* ---- STOP：停机（同时把目标方向也置为停止，否则会被重新拉起） ---- */
    else if (strcmp(cmd, "STOP") == 0 || strcmp(cmd, "stop") == 0)
    {
        ctrl_mode = MODE_UART;
        Motor_SetTarget(MOTOR_STOP, 0);
        Uart_SendText(MSG_OK_STOP);
    }
    /* ---- ADC：切回电位器控制 ---- */
    else if (strcmp(cmd, "ADC") == 0 || strcmp(cmd, "adc") == 0)
    {
        ctrl_mode = MODE_ADC;
        Uart_SendText(MSG_OK_ADC);
    }
    else
    {
        Uart_SendText(MSG_UNKNOWN);
    }
}

/**
  * @brief  上报当前档位给网关
  * @note   报文格式 Speed:x\r\n，x 为当前实际档位（含平滑过渡中的中间值）
  */
static void SendToUSART1(void)
{
    char data_buf[20];

    sprintf(data_buf, "Speed:%d\r\n", Motor_GetGear());
    HAL_UART_Transmit(&huart1, (uint8_t *)data_buf, strlen(data_buf), 100);
}

/**
  * @brief  刷新 OLED 四行显示
  * @note   每次都在固定位置覆盖写，不需要先清屏；
  *         方向/模式字符串等宽（3~4 字符），不会留下残影。
  */
static void OLED_Refresh(void)
{
    /* 第一行：模式与方向 */
    OLED_ShowString(0, 0, "M:");
    OLED_ShowString(12, 0, (ctrl_mode == MODE_ADC) ? "ADC " : "UART");
    OLED_ShowString(36, 0, "D:");
    if (Motor_GetDir() == MOTOR_FWD)      OLED_ShowString(48, 0, "FWD");
    else if (Motor_GetDir() == MOTOR_REV) OLED_ShowString(48, 0, "REV");
    else                                  OLED_ShowString(48, 0, "STP");

    /* 第二行：档位与占空比 */
    OLED_ShowString(0, 2, "G:");
    OLED_ShowNum(12, 2, Motor_GetGear(), 2);
    OLED_ShowString(24, 2, "/10 S:");
    OLED_ShowNum(60, 2, Motor_GetDuty(), 3);
    OLED_ShowString(78, 2, "%");

    /* 第三行：当前档位 */
    OLED_ShowString(0, 4, "Gear:");
    OLED_ShowNum(30, 4, Motor_GetGear(), 2);
    OLED_ShowString(42, 4, "/10");

    /* 第四行：ADC 原始值 */
    OLED_ShowString(0, 6, "ADC:");
    OLED_ShowNum(24, 6, adc_value, 4);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  uint32_t last_adc_tick   = 0;
  uint32_t last_motor_tick = 0;
  uint32_t last_ui_tick    = 0;

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  Motor_Init();                 /* 必须在 MX_ADC1_Init() 之后，见 motor.h 说明 */
  OLED_Init();

  /* 启动串口单字节中断接收 */
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

  /* 上电立刻显示初始界面，不用等主循环第一次刷新 */
  OLED_ShowString(0, 0, "M:ADC D:STP");
  OLED_ShowString(0, 2, "G: 0/10 S:  0%");
  OLED_ShowString(0, 4, "RPM:   0");
  OLED_ShowString(0, 6, "ADC:   0");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now = HAL_GetTick();

    /* ---- 1. 串口指令（JSON 优先，其次旧文本命令） ---- */
    process_usart1_data();

    /* ---- 2. ADC 采样与档位映射（每 25ms） ---- */
    if (now - last_adc_tick >= ADC_PERIOD_MS)
    {
      last_adc_tick = now;

      HAL_ADC_Start(&hadc1);
      if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
      {
        adc_value = HAL_ADC_GetValue(&hadc1);
      }

      if (ctrl_mode == MODE_ADC)
      {
        /* 12 位 ADC(0~4095) → 占空比 0~99 → 档位 0~9
         * （与原始实现一致：ADC 满量程也只能到 9 档） */
        uint8_t duty = (uint8_t)(adc_value * 100 / 4096);
        uint8_t gear = duty / 10;

        Motor_SetTarget((gear > 0) ? MOTOR_FWD : MOTOR_STOP, gear);
      }
    }

    /* ---- 3. 电机平滑变速（每 15ms） ---- */
    if (now - last_motor_tick >= MOTOR_TICK_MS)
    {
      last_motor_tick = now;
      Motor_SmoothUpdate();
    }

    /* ---- 4. OLED 刷新 + 状态上报（每 100ms） ---- */
    if (now - last_ui_tick >= UI_PERIOD_MS)
    {
      last_ui_tick = now;

      OLED_Refresh();
      SendToUSART1();
    }
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV8;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
}

/* USER CODE BEGIN 4 */

/**
  * @brief  串口1 接收完成回调（每收到一个字节触发一次）
  * @note   只做"攒一帧"这一件事：遇到 \r 或 \n 认为帧结束。
  *         清零 rx_idx 后，CRLF 里跟在 \r 后面的那个 \n 会因为 rx_idx==0
  *         而被自然忽略。真正的解析在主循环的 process_usart1_data() 中。
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (rx_byte == '\r' || rx_byte == '\n')
        {
            if (rx_idx > 0)
            {
                rx_buf[rx_idx] = '\0';
                rx_frame_ready = 1;
                rx_idx = 0;
            }
        }
        else if (rx_idx < sizeof(rx_buf) - 1)
        {
            rx_buf[rx_idx++] = (char)rx_byte;
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

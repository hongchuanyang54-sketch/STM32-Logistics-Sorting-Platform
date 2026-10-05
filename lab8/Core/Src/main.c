/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    main.c
 * @brief   传送带控制器（lab8）—— FreeRTOS 版
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
 * ── 移植到 RTOS 的关键变化 ──
 * 裸机版所有事都在一个主循环里轮转，其中最慢的 OLED_Refresh() 是阻塞的，
 * 实测要 ~0.5 秒（软件 I²C，一屏 43 个字符 = 387 次 START/STOP 事务）。
 * 结果就是注释里写的"电机 15ms 一拍"实际被拖成 ~500ms 一拍。
 *
 * 现在拆成 6 个任务，OLED 那条慢路径单独扔到最低优先级：
 *   TaskMotor  (High)        15ms   Motor_SmoothUpdate()  ← 唯一要求节拍准的
 *   TaskAdc    (AboveNormal) 25ms   App_SampleAdc()
 *   TaskUartRx (Normal)      事件   收字节 → 攒帧 → App_HandleLine()
 *   TaskReport (BelowNormal) 100ms  App_ReportSpeed()
 *   TaskUartTx (BelowNormal) 事件   唯一真正写 huart1 的任务
 *   TaskOled   (Low)         自定    App_RefreshOled()
 *
 * 本文件保留"应用逻辑"，freertos.c 里的任务体只是按节拍调用这里的 4 个
 * App_* 函数。这样 CubeMX 重新生成代码时业务逻辑都在 USER CODE
 * 区里，不会被冲掉。
 *
 * 模块划分：
 *   MOTOR/motor.c        电机状态机（平滑变速、安全换向、PWM 启停）
 *   OLED/oled.c          SSD1306 显示驱动
 *   SOFT_I2C/soft_i2c.c  软件模拟 I²C
 *   UART_SEND/uart_send.c 发送队列（串行化所有串口输出）
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "cmsis_os.h"
#include "gpio.h"
#include "tim.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "motor.h"
#include "oled.h"
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
/* 控制模式 */
typedef enum {
  MODE_ADC = 0, /* 由电位器 ADC 值决定转速 */
  MODE_UART = 1 /* 由串口指令决定转速   */
} CtrlMode_t;
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* ctrl_mode 只有 TaskUartRx 会写（经 App_HandleLine），其余任务只读。
   单字节读写在这颗 MCU 上是原子的，不需要加锁。
   adc_value 同理：TaskAdc 写、TaskOled 读，32 位对齐访问是原子的。 */
static volatile CtrlMode_t ctrl_mode =
    MODE_ADC; // 作用是标记当前的控制模式，初始值为 MODE_ADC，表示由电位器 ADC
              // 值决定转速。
static volatile uint32_t adc_value = 0; // 作用是存储当前的 ADC 值，初始值为 0。

/* 串口单字节接收缓冲（交给 HAL 使用）。
   裸机版那套 rx_buf/rx_idx/rx_frame_ready + 关中断拷贝已经删掉了：
   现在中断只把字节投进 uartRxQueue，攒帧由 TaskUartRx 用自己的私有缓冲完成，
   天然没有竞态。 */
uint8_t rx_byte;

/* 队列由 CubeMX 在 freertos.c 里创建
   （FREERTOS -> Tasks and Queues -> Queues） */
extern osMessageQueueId_t uartRxQueueHandle; /* 中断收字节 -> TaskUartRx */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
static void HandleLegacyCommand(
    const char *cmd); // 作用是处理旧文本命令（F<n> / R<n> / STOP /
                      // ADC），参数// cmd 是以 '\0' 结尾的一行报文。
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ---- 固定应答报文。长度一律用 strlen 取，不手写数字，避免数错 ---- */
static const char MSG_ERR_GEAR[] = "Error: Gear 1-10\r\n";
static const char MSG_OK_STOP[] = "OK: STOP\r\n";
static const char MSG_OK_ADC[] = "OK: ADC Mode\r\n";
static const char MSG_UNKNOWN[] = "Unknown Command\r\n";

/**
 * @brief  作用是处理旧文本命令（F<n> / R<n> / STOP / ADC）
 * @note   第一段是防回环过滤：本机自己发出的上报和应答会经 TX 串回 RX，
 *         不滤掉会造成死循环。
 */
static void HandleLegacyCommand(const char *cmd) {
  char res[32];
  int g;

  /* ---- 防回环 ---- */
  if (strncmp(cmd, "Speed:", 6) == 0 || strncmp(cmd, "OK:", 3) == 0 ||
      strncmp(cmd, "Error:", 6) == 0 || strncmp(cmd, "Unknown", 7) == 0 ||
      cmd[0] == '{') {
    return;
  }

  /* ---- F<n>：正转 n 档 ---- */
  if (cmd[0] == 'F' || cmd[0] == 'f') {
    g = atoi(cmd + 1); // atoi() 函数将字符串转换为整数，cmd + 1
                       // 表示跳过第一个字符 'F' 或 'f'，获取后面的数字部分。
    if (g >= 1 && g <= MOTOR_GEAR_MAX) {
      ctrl_mode =
          MODE_UART; // 这一步是将控制模式设置为
                     // MODE_UART，表示当前的控制模式由串口指令决定转速。
      Motor_SetTarget(MOTOR_FWD, (uint8_t)g);
      sprintf(
          res, "OK: FWD %d Gear\r\n",
          g); // 作用是将格式化的字符串写入 res 缓冲区，表示设置正转档位成功。
      Uart_SendText(res);
    } else {
      Uart_SendText(MSG_ERR_GEAR);
    }
  }
  /* ---- R<n>：反转 n 档 ---- */
  else if (cmd[0] == 'R' || cmd[0] == 'r') {
    g = atoi(cmd + 1);
    if (g >= 1 && g <= MOTOR_GEAR_MAX) {
      ctrl_mode = MODE_UART;
      Motor_SetTarget(MOTOR_REV, (uint8_t)g);
      sprintf(res, "OK: REV %d Gear\r\n", g);
      Uart_SendText(res);
    } else {
      Uart_SendText(MSG_ERR_GEAR);
    }
  }
  /* ---- STOP：停机（同时把目标方向也置为停止，否则会被重新拉起） ---- */
  else if (strcmp(cmd, "STOP") == 0 || strcmp(cmd, "stop") == 0) {
    ctrl_mode = MODE_UART;
    Motor_SetTarget(MOTOR_STOP, 0);
    Uart_SendText(MSG_OK_STOP);
  }
  /* ---- ADC：切回电位器控制 ---- */
  else if (strcmp(cmd, "ADC") == 0 || strcmp(cmd, "adc") == 0) {
    ctrl_mode = MODE_ADC;
    Uart_SendText(MSG_OK_ADC);
  } else {
    Uart_SendText(MSG_UNKNOWN);
  }
}

/**
 * @brief  处理一整帧串口数据（由 TaskUartRx 攒好一帧后调用）
 * @param  line 以 '\0' 结尾的一行报文
 * @note   裸机版要在这里关中断把 rx_buf 整个拷出来再解析；
 *         现在字节是队列一个个送过来的，缓冲是任务的私有变量，这步可以整个删掉。
 */
void App_HandleLine(const char *line) { // 作用是处理一整帧串口数据，参数 line
                                        // 是以 '\0' 结尾的一行报文。
  char speed_str[10];
  int speed;

  /* ---- 1. JSON 远程调速：{"Speed":"x"} ---- */
  /* 宽度必须写成 %9[...]：speed_str 只有 10 字节（含结尾 '\0'），
     不加宽度限制的话，上位机发个 {"Speed":"999999999999"} 就能顺着栈
     写出 40 多字节，砸掉本函数的返回地址。 */
  if (sscanf(line, "{\"Speed\":\"%9[^\"]\"}", speed_str) ==
      1) { // sscanf() 函数从 line 中按指定格式提取字符串，%9[^\"] 表示最多读取
           // 9 个非引号字符，存入 speed_str
           // 中。返回值为成功匹配的项数，如果成功匹配到一个项，则返回 1。
    speed = atoi(speed_str);

    if (speed >= 0 && speed <= MOTOR_GEAR_MAX) {
      ctrl_mode = MODE_UART; /* 收到远程指令后锁定串口模式，防止 ADC 覆盖 */

      if (speed == 0) {
        Motor_SetTarget(MOTOR_STOP, 0);
      } else {
        Motor_SetTarget(MOTOR_FWD, (uint8_t)speed); /* 默认正转 */
      }
    }
    return;
  }

  /* ---- 2. 旧文本命令 ---- */
  HandleLegacyCommand(line);
}

/**
 * @brief  采一次 ADC，并在 ADC 模式下更新目标转速
 * @note   由 TaskAdc 每 25ms 调用
 */
void App_SampleAdc(void) {
  HAL_ADC_Start(&hadc1); // 调用 HAL_ADC_Start() 函数启动 ADC 转换，参数 &hadc1
                         // 是 ADC 句柄，表示要操作的 ADC 实例。
  if (HAL_ADC_PollForConversion(&hadc1, 10) ==
      HAL_OK) { // HAL_ADC_PollForConversion是一个阻塞函数，用于等待 ADC
                // 转换完成。参数 &hadc1 是 ADC 句柄，10
                // 是超时时间（单位：毫秒）。如果转换成功完成，返回 HAL_OK。
    adc_value = HAL_ADC_GetValue(&hadc1);
  }

  /* UART 模式下只采样、只显示，不碰电机 ——
     这就是手册里说的"收到串口信息后 ADC 数据被暂时屏蔽"的实现 */
  if (ctrl_mode == MODE_ADC) {
    /* 12 位 ADC(0~4095) → 占空比 0~99 → 档位 0~9
     * （与原始实现一致：ADC 满量程也只能到 9 档） */
    uint8_t duty = (uint8_t)(adc_value * 100 / 4096);
    uint8_t gear = duty / 10;

    Motor_SetTarget((gear > 0) ? MOTOR_FWD : MOTOR_STOP, gear);
  }
}

/**
 * @brief  上报当前档位给网关
 * @note   由 TaskReport 每 100ms 调用。
 *         报文格式 Speed:x\r\n，x 为当前实际档位（含平滑过渡中的中间值）。
 *         注意这里只是投递到发送队列，真正写串口的是 TaskUartTx。
 */
void App_ReportSpeed(void) {
  char data_buf[20]; // 作用是存储要发送的报文，大小为 20 字节。

  sprintf(data_buf, "Speed:%d\r\n",
          Motor_GetGear()); // sprintf() 函数将格式化的字符串写入 data_buf
                            // 缓冲区，表示当前实际档位。
  Uart_SendText(data_buf);
}

/**
 * @brief  刷新 OLED 四行显示
 * @note   由 TaskOled 调用（最低优先级，慢慢画）。
 *         每次都在固定位置覆盖写，不需要先清屏；
 *         方向/模式字符串等宽（3~4 字符），不会留下残影。
 */
void App_RefreshOled(void) {
  /* 第一行：模式与方向 */
  OLED_ShowString(0, 0, "M:");
  OLED_ShowString(12, 0, (ctrl_mode == MODE_ADC) ? "ADC " : "UART");
  OLED_ShowString(36, 0, "D:");
  if (Motor_GetDir() == MOTOR_FWD)
    OLED_ShowString(48, 0, "FWD");
  else if (Motor_GetDir() == MOTOR_REV)
    OLED_ShowString(48, 0, "REV");
  else
    OLED_ShowString(48, 0, "STP");

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
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  Motor_Init(); /* 必须在 MX_ADC1_Init() 之后，见 motor.h 说明 */
  OLED_Init();  /* 内部要等 OLED 上电稳定并清屏，约 0.3 秒 */

  /* 串口接收不在这里使能！
     要等到 uartRxQueue 建好之后（见 freertos.c 的 USER CODE BEGIN
     RTOS_THREADS）。 在这里调 HAL_UART_Receive_IT 的话，从中断使能到
     MX_FREERTOS_Init() 建好队列 之间有个几微秒的窗口：这期间中断回调会拿 NULL
     句柄调 osMessageQueuePut， 字节被静默丢掉，主机的第一帧就废了。 */

  /* 这里【不要】再画一次开机界面。TaskOled 一启动就会调 App_RefreshOled()
     画出同样的内容，多写一处只会制造不一致 —— 原来那两处的第四行
     一处写 "RPM:" 一处写 "Gear:"，就是这么来的。 */
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize(); /* Call init function for freertos objects (in
                           cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  /* 控制权已经交给调度器：osKernelStart() 不会返回。
     原先的主循环（串口解析 / ADC / 电机 / OLED）已经拆成
     freertos.c 里的 6 个任务。 */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV8;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief  串口1 接收完成回调（每收到一个字节触发一次）
 * @note   中断里只做一件事：把字节投进 uartRxQueue，立刻返回。
 *         攒帧（判 \r \n）现在由 TaskUartRx 负责。
 *
 *         osMessageQueuePut 是 ISR 安全的（靠 __get_IPSR() 自动分辨），
 *         但在中断里超时参数必须是 0。
 *
 *         ⚠ 本函数在 USART1 中断里跑，它的抢占优先级必须是 5
 *         （configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY），
 *         见 usart.c 里的 HAL_NVIC_SetPriority。优先级比 5 高（数值更小）
 *         的 ISR 调 FreeRTOS API 是非法的。
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
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
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
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

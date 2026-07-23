/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 终极修复版：完美双模式隔离 + OLED显示转速(RPM) + STOP彻底修复
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

/* USER CODE BEGIN PD */
#define MODE_ADC   0
#define MODE_UART  1
#define MOTOR_STOP 0
#define MOTOR_FWD  1
#define MOTOR_REV  2
#define GEAR_MAX   10
#define OLED_ADDR  0x78
#define GEAR_STEP  5
#define MAX_RPM    3000
/* USER CODE END PD */

/* USER CODE BEGIN PM */
#define SCL_H()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET)
#define SCL_L()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET)
#define SDA_H()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)
#define SDA_L()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define SDA_IN()  HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4)
/* USER CODE END PM */

void SystemClock_Config(void);
void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t size);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size);

/* USER CODE BEGIN PV */
uint32_t adc_value = 0;
uint8_t  pwm_duty = 0;
uint8_t  ctrl_mode = MODE_UART;
uint32_t adc_baseline = 0;

uint8_t motor_dir = MOTOR_STOP;
uint8_t motor_gear = 0;
uint8_t target_dir = MOTOR_STOP;
uint8_t target_gear = 0;

// 【新增】PWM是否正在运行的标志
uint8_t pwm_running = 0;

uint8_t  rx_byte = 0;
char     rx_buf[16] = {0};
uint8_t  rx_idx = 0;
uint8_t  rx_complete = 0;

/* ===== 物联网实验：JSON 双向通信新增变量 ===== */
uint8_t  SpeedLevel = 0;        // 当前转速档位（0~10），与 motor_gear 保持同步
uint16_t PWM_num    = 0;        // PWM 占空比计数值（0~100），与 pwm_duty 保持同步
uint8_t  adflag     = 0;        // OLED 显示更新标志位（1 = 需要刷新）
uint8_t  PWMflag    = 0;        // PWM 输出更新标志位（1 = 有新的远程调速指令）
uint8_t  rflag1     = 0;        // 串口1 JSON 命令接收完成标志位
uint8_t  buffer1[50];           // 串口1 JSON 命令接收缓冲区

static const uint8_t F6x8[][6] = {
  {0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x2f,0x00,0x00},{0x00,0x00,0x07,0x00,0x07,0x00},
  {0x00,0x14,0x7f,0x14,0x7f,0x14},{0x00,0x24,0x2a,0x7f,0x2a,0x12},{0x00,0x23,0x13,0x08,0x64,0x62},
  {0x00,0x36,0x49,0x55,0x22,0x50},{0x00,0x00,0x05,0x03,0x00,0x00},{0x00,0x00,0x1c,0x22,0x41,0x00},
  {0x00,0x00,0x41,0x22,0x1c,0x00},{0x00,0x08,0x2a,0x1c,0x2a,0x08},{0x00,0x08,0x08,0x3e,0x08,0x08},
  {0x00,0x00,0x50,0x30,0x00,0x00},{0x00,0x08,0x08,0x08,0x08,0x08},{0x00,0x00,0x60,0x60,0x00,0x00},
  {0x00,0x20,0x10,0x08,0x04,0x02},{0x00,0x3e,0x51,0x49,0x45,0x3e},{0x00,0x00,0x42,0x7f,0x40,0x00},
  {0x00,0x42,0x61,0x51,0x49,0x46},{0x00,0x21,0x41,0x45,0x4b,0x31},{0x00,0x18,0x14,0x12,0x7f,0x10},
  {0x00,0x27,0x45,0x45,0x45,0x39},{0x00,0x3c,0x4a,0x49,0x49,0x30},{0x00,0x01,0x71,0x09,0x05,0x03},
  {0x00,0x36,0x49,0x49,0x49,0x36},{0x00,0x06,0x49,0x49,0x29,0x1e},{0x00,0x00,0x36,0x36,0x00,0x00},
  {0x00,0x00,0x56,0x36,0x00,0x00},{0x00,0x00,0x08,0x14,0x22,0x41},{0x00,0x14,0x14,0x14,0x14,0x14},
  {0x00,0x41,0x22,0x14,0x08,0x00},{0x00,0x02,0x01,0x51,0x09,0x06},{0x00,0x32,0x49,0x79,0x41,0x3e},
  {0x00,0x7e,0x11,0x11,0x11,0x7e},{0x00,0x7f,0x49,0x49,0x49,0x36},{0x00,0x3e,0x41,0x41,0x41,0x22},
  {0x00,0x7f,0x41,0x41,0x22,0x1c},{0x00,0x7f,0x49,0x49,0x49,0x41},{0x00,0x7f,0x09,0x09,0x01,0x01},
  {0x00,0x3e,0x41,0x41,0x51,0x32},{0x00,0x7f,0x08,0x08,0x08,0x7f},{0x00,0x00,0x41,0x7f,0x41,0x00},
  {0x00,0x20,0x40,0x41,0x3f,0x01},{0x00,0x7f,0x08,0x14,0x22,0x41},{0x00,0x7f,0x40,0x40,0x40,0x40},
  {0x00,0x7f,0x02,0x04,0x02,0x7f},{0x00,0x7f,0x04,0x08,0x10,0x7f},{0x00,0x3e,0x41,0x41,0x41,0x3e},
  {0x00,0x7f,0x09,0x09,0x09,0x06},{0x00,0x3e,0x41,0x51,0x21,0x5e},{0x00,0x7f,0x09,0x19,0x29,0x46},
  {0x00,0x46,0x49,0x49,0x49,0x31},{0x00,0x01,0x01,0x7f,0x01,0x01},{0x00,0x3f,0x40,0x40,0x40,0x3f},
  {0x00,0x1f,0x20,0x40,0x20,0x1f},{0x00,0x7f,0x20,0x18,0x20,0x7f},{0x00,0x63,0x14,0x08,0x14,0x63},
  {0x00,0x03,0x04,0x78,0x04,0x03},{0x00,0x61,0x51,0x49,0x45,0x43},{0x00,0x00,0x00,0x7f,0x41,0x41},
  {0x00,0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7f,0x00,0x00},{0x00,0x04,0x02,0x01,0x02,0x04},
  {0x00,0x40,0x40,0x40,0x40,0x40},{0x00,0x00,0x01,0x02,0x04,0x00},{0x00,0x20,0x54,0x54,0x54,0x78},
  {0x00,0x7f,0x48,0x44,0x44,0x38},{0x00,0x38,0x44,0x44,0x44,0x20},{0x00,0x38,0x44,0x44,0x48,0x7f},
  {0x00,0x38,0x54,0x54,0x54,0x18},{0x00,0x08,0x7e,0x09,0x01,0x02},{0x00,0x08,0x14,0x54,0x54,0x3c},
  {0x00,0x7f,0x08,0x04,0x04,0x78},{0x00,0x00,0x44,0x7d,0x40,0x00},{0x00,0x20,0x40,0x44,0x3d,0x00},
  {0x00,0x00,0x7f,0x10,0x28,0x44},{0x00,0x00,0x41,0x7f,0x40,0x00},{0x00,0x7c,0x04,0x18,0x04,0x78},
  {0x00,0x7c,0x08,0x04,0x04,0x78},{0x00,0x38,0x44,0x44,0x44,0x38},{0x00,0x7c,0x14,0x14,0x14,0x08},
  {0x00,0x08,0x14,0x14,0x18,0x7c},{0x00,0x7c,0x08,0x04,0x04,0x08},{0x00,0x48,0x54,0x54,0x54,0x20},
  {0x00,0x04,0x3f,0x44,0x40,0x20},{0x00,0x3c,0x40,0x30,0x40,0x3c},{0x00,0x1c,0x20,0x40,0x20,0x1c},
  {0x00,0x3c,0x40,0x30,0x40,0x3c},{0x00,0x44,0x28,0x10,0x28,0x44},{0x00,0x0c,0x50,0x50,0x50,0x3c},
  {0x00,0x44,0x64,0x54,0x4c,0x44},{0x00,0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x00,0x7f,0x00,0x00},
  {0x00,0x00,0x41,0x36,0x08,0x00},{0x00,0x08,0x04,0x08,0x10,0x08}
};
/* USER CODE END PV */

/* USER CODE BEGIN 0 */

/* ===== 物联网实验：函数前向声明 ===== */
void SendToUSART1(void);
void process_usart1_data(void);

static void I2C_Delay(void) { volatile uint32_t i = 100; while (i--) { __NOP(); } }
static void I2C_Start(void) { SDA_H(); SCL_H(); I2C_Delay(); SDA_L(); I2C_Delay(); SCL_L(); }
static void I2C_Stop(void) { SDA_L(); SCL_H(); I2C_Delay(); SDA_H(); I2C_Delay(); }
static uint8_t I2C_WaitAck(void) {
  uint8_t ack; SDA_H(); I2C_Delay(); SCL_H(); I2C_Delay();
  ack = SDA_IN(); SCL_L(); I2C_Delay(); return ack;
}
static void I2C_SendByte(uint8_t dat) {
  for(uint8_t i=0; i<8; i++) {
    if(dat & 0x80) SDA_H(); else SDA_L();
    dat <<= 1; I2C_Delay(); SCL_H(); I2C_Delay(); SCL_L(); I2C_Delay();
  }
}
static void OLED_WR_Byte(uint8_t dat, uint8_t cmd) {
  I2C_Start(); I2C_SendByte(OLED_ADDR); I2C_WaitAck();
  I2C_SendByte(cmd ? 0x40 : 0x00); I2C_WaitAck();
  I2C_SendByte(dat); I2C_WaitAck(); I2C_Stop();
}
static void OLED_SetPos(uint8_t x, uint8_t y) {
  OLED_WR_Byte(0xB0 + y, 0);
  OLED_WR_Byte(0x00 + (x & 0x0F), 0);
  OLED_WR_Byte(0x10 + (x >> 4), 0);
}
void OLED_Init(void) {
  HAL_Delay(100);
  OLED_WR_Byte(0xAE,0); OLED_WR_Byte(0x20,0); OLED_WR_Byte(0x10,0);
  OLED_WR_Byte(0xB0,0); OLED_WR_Byte(0xC8,0);
  OLED_WR_Byte(0x00,0); OLED_WR_Byte(0x10,0); OLED_WR_Byte(0x40,0);
  OLED_WR_Byte(0x81,0); OLED_WR_Byte(0xFF,0);
  OLED_WR_Byte(0xA1,0); OLED_WR_Byte(0xA6,0);
  OLED_WR_Byte(0xA8,0); OLED_WR_Byte(0x3F,0);
  OLED_WR_Byte(0xA4,0); OLED_WR_Byte(0xD3,0); OLED_WR_Byte(0x00,0);
  OLED_WR_Byte(0xD5,0); OLED_WR_Byte(0xF0,0);
  OLED_WR_Byte(0xD9,0); OLED_WR_Byte(0x22,0);
  OLED_WR_Byte(0xDA,0); OLED_WR_Byte(0x12,0);
  OLED_WR_Byte(0xDB,0); OLED_WR_Byte(0x20,0);
  OLED_WR_Byte(0x8D,0); OLED_WR_Byte(0x14,0);
  OLED_WR_Byte(0xAF,0);
  OLED_Clear();
}
void OLED_Clear(void) {
  for(uint8_t i=0; i<8; i++) {
    OLED_WR_Byte(0xB0+i, 0);
    OLED_WR_Byte(0x00, 0); OLED_WR_Byte(0x10, 0);
    for(uint8_t j=0; j<128; j++) OLED_WR_Byte(0x00, 1);
  }
}

void OLED_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t size) {
  uint8_t c = ch - 32;
  if(x > 127) { x=0; y++; }
  OLED_SetPos(x, y);
  for(uint8_t i=0; i<6; i++) OLED_WR_Byte(F6x8[c][i], 1);
}

void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size) {
  while(*str) {
      OLED_ShowChar(x, y, *str, size);
      x += 6;
      str++;
  }
}

void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size) {
  char buf[16]; sprintf(buf, "%lu", num);
  uint8_t l = strlen(buf);
  for(uint8_t i=0; i<len-l; i++) { OLED_ShowChar(x, y, ' ', size); x += 6; }
  OLED_ShowString(x, y, buf, size);
}

// 【新增】安全启动PWM：只在未运行时启动
static void PWM_SafeStart(void) {
  if(!pwm_running) {
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    pwm_running = 1;
  }
}

// 【新增】安全停止PWM：彻底关闭PWM输出
static void PWM_SafeStop(void) {
  if(pwm_running) {
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
    pwm_running = 0;
  }
}

void Motor_DirectSet(uint8_t dir, uint8_t gear) {
  if(dir == MOTOR_STOP || gear == 0) {
    // 【修复】停止时：IN1/IN2全部拉低 + 彻底关闭PWM
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
    PWM_SafeStop();
    motor_dir = MOTOR_STOP;
    motor_gear = 0;
    pwm_duty = 0;
    return;
  }

  // 需要运行：先确保PWM已启动
  PWM_SafeStart();

  if(dir == MOTOR_FWD) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
  } else if(dir == MOTOR_REV) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
  }

  pwm_duty = gear * 10;
  uint32_t cmp = (pwm_duty * 99) / 100;
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, cmp);

  motor_dir = dir;
  motor_gear = gear;
}

void Motor_SmoothUpdate(void) {
  // 【修复】目标为停止：直接减速到0然后彻底关闭
  if(target_dir == MOTOR_STOP || target_gear == 0) {
    if(motor_gear > 0) {
      // 平滑减速
      motor_gear = (motor_gear > GEAR_STEP) ? (motor_gear - GEAR_STEP) : 0;
      if(motor_gear == 0) {
        // 减到0了：彻底关闭
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
        PWM_SafeStop();
        motor_dir = MOTOR_STOP;
        pwm_duty = 0;
      } else {
        pwm_duty = motor_gear * 10;
        uint32_t cmp = (pwm_duty * 99) / 100;
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, cmp);
      }
      return;
    } else {
      // 已经是0了，确保关闭
      if(motor_dir != MOTOR_STOP) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
        PWM_SafeStop();
        motor_dir = MOTOR_STOP;
        pwm_duty = 0;
      }
      return;
    }
  }

  // 目标需要运行：确保PWM已启动
  PWM_SafeStart();

  // 方向不同：先减速到0再换向
  if(target_dir != motor_dir && motor_dir != MOTOR_STOP) {
    if(motor_gear > 0) {
      motor_gear = (motor_gear > GEAR_STEP) ? (motor_gear - GEAR_STEP) : 0;
      uint32_t cmp = (motor_gear * 10 * 99) / 100;
      __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, cmp);
      pwm_duty = motor_gear * 10;
      return;
    }
    else {
      // 速度到0：换向
      if(target_dir == MOTOR_FWD) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
      } else if(target_dir == MOTOR_REV) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
      }
      motor_dir = target_dir;
      return;
    }
  }

  // 从停止状态启动：设置方向
  if(motor_dir == MOTOR_STOP) {
    if(target_dir == MOTOR_FWD) {
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
    } else if(target_dir == MOTOR_REV) {
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
    }
    motor_dir = target_dir;
  }

  // 同方向：按步长调速
  if(motor_gear < target_gear) {
    motor_gear += GEAR_STEP;
    if(motor_gear > target_gear) motor_gear = target_gear;
  } else if(motor_gear > target_gear) {
    motor_gear -= GEAR_STEP;
    if(motor_gear < target_gear) motor_gear = target_gear;
  }

  pwm_duty = motor_gear * 10;
  uint32_t cmp = (pwm_duty * 99) / 100;
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, cmp);
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  OLED_Init();
  // 【修改】不再在初始化时启动PWM，由Motor函数按需启动
  // HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);  // 已删除
  pwm_running = 0;
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

  /* 立即显示初始界面，无需等待主循环首次刷新 */
  OLED_ShowString(0, 0, "M:ADC D:STP", 8);
  OLED_ShowString(0, 2, "G: 0/10 S:  0%", 8);
  OLED_ShowString(0, 4, "RPM:   0",     8);
  OLED_ShowString(0, 6, "ADC:   0",     8);

  target_dir = MOTOR_STOP;
  target_gear = 0;
  ctrl_mode = MODE_ADC;   // 默认电位器模式，上电即可用旋钮调速

  uint32_t last_oled_tick = 0;
  uint32_t last_adc_tick = 0;
  uint32_t last_motor_tick = 0;
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN 3 */
    uint32_t current_tick = HAL_GetTick();

    // ================= 0. JSON 远程调速指令处理（优先于旧协议） =================
    process_usart1_data();

    // ================= 1. 串口指令处理（旧协议: F/R/STOP/ADC） =================
    if(rx_complete) {
      // 【防回环】忽略本机主动上报和 JSON 应答，避免 TX→RX 回环死循环
      if(strncmp(rx_buf, "Speed:", 6) == 0 ||
         strncmp(rx_buf, "OK:",    3) == 0 ||
         strncmp(rx_buf, "Error:", 6) == 0 ||
         strncmp(rx_buf, "Unknown", 7) == 0 ||
         rx_buf[0] == '{') {                     /* 跳过 JSON 格式命令 */
        rx_idx = 0;
        rx_buf[0] = '\0';
        rx_complete = 0;
      }
      else if(rx_buf[0] == 'F' || rx_buf[0] == 'f') {
        int g = atoi(rx_buf + 1);
        if(g >= 1 && g <= GEAR_MAX) {
          ctrl_mode = MODE_UART;
          target_dir = MOTOR_FWD; target_gear = g;
          char res[32]; sprintf(res, "OK: FWD %d Gear\r\n", g);
          HAL_UART_Transmit(&huart1, (uint8_t*)res, strlen(res), 100);
        } else {
          HAL_UART_Transmit(&huart1, (uint8_t*)"Error: Gear 1-10\r\n", 17, 100);
        }
      }
      else if(rx_buf[0] == 'R' || rx_buf[0] == 'r') {
        int g = atoi(rx_buf + 1);
        if(g >= 1 && g <= GEAR_MAX) {
          ctrl_mode = MODE_UART;
          target_dir = MOTOR_REV; target_gear = g;
          char res[32]; sprintf(res, "OK: REV %d Gear\r\n", g);
          HAL_UART_Transmit(&huart1, (uint8_t*)res, strlen(res), 100);
        } else {
          HAL_UART_Transmit(&huart1, (uint8_t*)"Error: Gear 1-10\r\n", 17, 100);
        }
      }
      else if(strcmp(rx_buf, "STOP") == 0 || strcmp(rx_buf, "stop") == 0) {
        // 【关键修复】STOP同时设置 target_dir = MOTOR_STOP
        ctrl_mode = MODE_UART;
        target_dir = MOTOR_STOP;
        target_gear = 0;
        HAL_UART_Transmit(&huart1, (uint8_t*)"OK: STOP\r\n", 10, 100);
      }
      else if(strcmp(rx_buf, "ADC") == 0 || strcmp(rx_buf, "adc") == 0) {
        ctrl_mode = MODE_ADC;
        adc_baseline = adc_value;
        HAL_UART_Transmit(&huart1, (uint8_t*)"OK: ADC Mode\r\n", 14, 100);
      }
      else {
        HAL_UART_Transmit(&huart1, (uint8_t*)"Unknown Command\r\n", 17, 100);
      }

      rx_idx = 0;
      rx_buf[0] = '\0';
      rx_complete = 0;
    }

    // ================= 2. ADC 采样 =================
    if (current_tick - last_adc_tick >= 25) {
      last_adc_tick = current_tick;

      HAL_ADC_Start(&hadc1);
      if(HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        adc_value = HAL_ADC_GetValue(&hadc1);
      }

      if(ctrl_mode == MODE_ADC) {
        uint8_t duty = (uint8_t)(adc_value * 100 / 4096);
        target_gear = duty / 10;
        if(target_gear > 10) target_gear = 10;
        if(target_gear > 0) {
          target_dir = MOTOR_FWD;
        } else {
          target_dir = MOTOR_STOP;
        }
      }
    }

    // ================= 3. 电机平滑更新 (每 15ms) =================
    if (current_tick - last_motor_tick >= 15) {
      last_motor_tick = current_tick;
      Motor_SmoothUpdate();

      /* 【新增】每次电机更新后同步物联网上报变量 */
      SpeedLevel = motor_gear;          // 同步实际档位（含平滑过渡中的中间值）
      PWM_num    = pwm_duty;            // 同步实际 PWM 占空比
      PWMflag    = 0;                   // 清除 PWM 更新标志（电机已响应）
    }

    // ================= 4. OLED 刷新 + 状态上行上报 (每 100ms) =================
    if (current_tick - last_oled_tick >= 100) {
      last_oled_tick = current_tick;

      // 第一行：模式与方向
      OLED_ShowString(0, 0, "M:", 8);
      OLED_ShowString(12, 0, ctrl_mode==MODE_ADC ? "ADC " : "UART", 8);
      OLED_ShowString(36, 0, "D:", 8);
      if(motor_dir == MOTOR_FWD) OLED_ShowString(48, 0, "FWD", 8);
      else if(motor_dir == MOTOR_REV) OLED_ShowString(48, 0, "REV", 8);
      else OLED_ShowString(48, 0, "STP", 8);

      // 第二行：挡位与占空比
      OLED_ShowString(0, 2, "G:", 8);
      OLED_ShowNum(12, 2, motor_gear, 2, 8);
      OLED_ShowString(24, 2, "/10 S:", 8);
      OLED_ShowNum(60, 2, pwm_duty, 3, 8);
      OLED_ShowString(78, 2, "%", 8);

      // 第三行：当前档位 (0~10)
      OLED_ShowString(0, 4, "Gear:", 8);
      OLED_ShowNum(30, 4, motor_gear, 2, 8);
      OLED_ShowString(42, 4, "/10", 8);

      // 第四行：ADC原始值
      OLED_ShowString(0, 6, "ADC:", 8);
      OLED_ShowNum(24, 6, adc_value, 4, 8);

      /* 【新增】周期性上报速度档位至 Python 网关 */
      SendToUSART1();

      /* 【新增】清除 ADC/显示更新标志 */
      adflag = 0;
    }
    /* USER CODE END 3 */
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

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
 * @brief  串口1 接收中断回调（每收到一个字节触发）
 * @note   同时服务于旧协议（rx_buf / rx_complete）和新 JSON 协议（buffer1 / rflag1）
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {
    if (rx_byte == '\r' || rx_byte == '\n') {
      if (rx_idx > 0) {
        rx_buf[rx_idx] = '\0';               /* 字符串终止符 */

        /* JSON 协议始终接收，不受旧协议 rx_complete 状态影响 */
        memcpy(buffer1, rx_buf, rx_idx + 1); /* 包含 '\0' 终止符 */
        rflag1 = 1;

        /* 旧协议仅在未处理完时置位，避免覆盖未读数据 */
        if (!rx_complete) {
          rx_complete = 1;
        }
      }
    } else {
      if (rx_idx < sizeof(rx_buf) - 1) {
        rx_buf[rx_idx++] = rx_byte;
      }
    }
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);  /* 重新使能中断接收 */
  }
}

/**
 * @brief  主动上报当前速度档位至 Python 网关
 * @note   上报格式：Speed:x\r\n（例如 Speed:5\r\n）
 *         在主循环的 OLED 刷新块中周期性调用（每 100ms 一次）
 */
void SendToUSART1(void)
{
  char data_buf[60];
  sprintf(data_buf, "Speed:%d\r\n", SpeedLevel);
  HAL_UART_Transmit(&huart1, (uint8_t*)data_buf, strlen(data_buf), 100);
}

/**
 * @brief  解析 Python 网关下发的 JSON 远程调速指令
 * @note   指令格式：{"Speed":"x"}\r\n（x 为 0~10 的目标档位字符串）
 *         解析成功后更新 SpeedLevel、PWM_num，并同步电机控制目标值
 */
void process_usart1_data(void)
{
  if (rflag1 == 0) return;      // 无新数据到达，直接返回

  char speed_str[10];

  // 使用 sscanf 从 JSON 中提取速度字符串（例如 {"Speed":"7"} → "7"）
  if (sscanf((const char*)buffer1, "{\"Speed\":\"%[^\"]\"}", speed_str) == 1)
  {
    int speed = atoi(speed_str);        // 字符串转整型数值

    // 合法性校验：档位必须在 0~10 范围内
    if (speed >= 0 && speed <= 10)
    {
      SpeedLevel = speed;               // 更新当前档位
      PWM_num    = SpeedLevel * 10;     // 档位 0~10 线性映射为 PWM 占空比 0~100

      // 同步更新电机平滑控制系统的目标值
      if (speed == 0)
      {
        target_dir  = MOTOR_STOP;
        target_gear = 0;
      }
      else
      {
        target_dir  = MOTOR_FWD;        // 默认正转，如需反转可扩展 JSON 字段
        target_gear = speed;
      }
      ctrl_mode = MODE_UART;            // 收到远程指令后锁定串口模式，防止 ADC 覆盖

      adflag  = 1;                      // 置位：触发 OLED 屏幕刷新
      PWMflag = 1;                      // 置位：标记有新调速指令到达

      // JSON 解析成功，清除旧协议标志避免重复处理
      rx_complete = 0;
      rx_idx = 0;
      rx_buf[0] = '\0';
    }
  }

  // 无论解析成功与否，都清除 JSON 协议标志并清空缓冲区
  rflag1 = 0;
  memset(buffer1, 0, sizeof(buffer1));
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

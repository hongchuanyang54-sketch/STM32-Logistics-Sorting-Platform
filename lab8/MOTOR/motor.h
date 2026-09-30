#ifndef __MOTOR_H
#define __MOTOR_H

#include "main.h"

/*
 * 直流电机控制（L298N）
 *   PA1 = IN1、PA2 = IN2   → 方向
 *   PA0 = TIM2_CH1         → ENA 使能，PWM 调速
 *
 * 档位 0~10，每档 10% 占空比。档位变化不是瞬间完成的：
 * 每 MOTOR_TICK_MS 毫秒调用一次 Motor_SmoothUpdate()，
 * 每次最多变化 MOTOR_GEAR_STEP 档，实现缓增缓减。
 */

/* 方向 */
#define MOTOR_STOP   0
#define MOTOR_FWD    1
#define MOTOR_REV    2

/* 档位上限（0 = 停止） */
#define MOTOR_GEAR_MAX   10

/* 平滑变速参数：15ms 一步、一步 5 档 */
#define MOTOR_TICK_MS    15
#define MOTOR_GEAR_STEP  5

/* 方向控制引脚 */
#define MOTOR_IN1_PIN  GPIO_PIN_1
#define MOTOR_IN2_PIN  GPIO_PIN_2
#define MOTOR_PORT     GPIOA

/**
  * @brief  初始化方向引脚
  * @note   必须在 MX_ADC1_Init() 之后调用 ——
  *         HAL_ADC_MspInit() 会把 PA1/PA2 一并配成模拟输入，
  *         本函数再把它们改成推挽输出。
  *         本函数不会启动 PWM，PWM 由 Motor_SmoothUpdate() 按需启停。
  */
void Motor_Init(void);

/**
  * @brief  设定目标方向与档位（立即返回，实际转速由 Motor_SmoothUpdate 逐步逼近）
  * @param  dir  MOTOR_STOP / MOTOR_FWD / MOTOR_REV
  * @param  gear 0~MOTOR_GEAR_MAX，超出会被截断
  */
void Motor_SetTarget(uint8_t dir, uint8_t gear);

/**
  * @brief  平滑变速状态机，每 MOTOR_TICK_MS 毫秒调用一次
  * @note   负责：缓慢升降档、换向时先减速到 0、PWM 的按需启停
  */
void Motor_SmoothUpdate(void);

/* 当前实际状态（平滑过渡中的中间值，可直接用于显示与上报） */
uint8_t Motor_GetGear(void);
uint8_t Motor_GetDir(void);
uint8_t Motor_GetDuty(void);

#endif

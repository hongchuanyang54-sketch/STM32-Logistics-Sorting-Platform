#include "motor.h"
#include "tim.h"

/* ============================================================
 * 模块私有状态
 *   motor_*  = 当前实际值（平滑过渡过程中会逐档变化）
 *   target_* = 目标值（由 Motor_SetTarget 设定）
 * ============================================================ */
static uint8_t motor_dir = MOTOR_STOP; /* 当前实际方向 */
static uint8_t motor_gear = 0;         /* 当前实际档位 0~10 */
static uint8_t pwm_duty = 0;           /* 当前占空比 0~100 = 档位 × 10 */

/* 目标方向与档位打包成一个 16 位量：高字节方向、低字节档位。
   为什么不沿用两个 uint8_t —— 裸机版里 Motor_SetTarget() 和
   Motor_SmoothUpdate() 都在主循环里跑，天然不会互相打断；
   移植到 RTOS 后前者来自 TaskUartRx / TaskAdc，后者在 TaskMotor，
   写两个字节不再原子：中途被抢占会让 TaskMotor 读到
   "新方向 + 旧档位"的混合值，凭空抖动一次换向。
   打包成 16 位之后，Cortex-M3 上一次对齐的 16 位写入就是原子的。 */
static volatile uint16_t target_pack = 0;

static uint8_t pwm_running = 0; /* PWM 是否已启动 */

/**
 * @brief  安全启动 PWM：只在未运行时启动
 * @note   重复调用 HAL_TIM_PWM_Start 会让 HAL 返回错误状态，
 *         因此用 pwm_running 做一次软件开关。
 */
static void PWM_SafeStart(
    void) { // 作用是安全启动 PWM，只在未运行时启动。重复调用 HAL_TIM_PWM_Start
            // 会让 HAL 返回错误状态，因此用 pwm_running 做一次软件开关。
  if (!pwm_running) {
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    pwm_running = 1;
  }
}

/**
 * @brief  安全停止 PWM：先清零比较值再关闭输出通道
 */
static void PWM_SafeStop(void) {
  if (pwm_running) {
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
    pwm_running = 0;
  }
}

/**
 * @brief  把档位换算成 TIM2 比较值并写寄存器
 * @note   TIM2 的 ARR = 100-1，所以比较值范围 0~99。
 *         档位 10 → 占空比 100% → 比较值 99。
 */
static void
PWM_ApplyGear(uint8_t gear) { // 作用是把档位换算成 TIM2 比较值并写寄存器。TIM2
                              // 的 ARR = 100-1，所以比较值范围 0~99。
  pwm_duty = gear * 10;
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (uint32_t)(pwm_duty * 99) / 100);
}

/**
 * @brief  直接设置 IN1/IN2 电平
 */
static void Motor_ApplyDirection(
    uint8_t dir) { // 作用是直接设置 IN1/IN2 电平，参数 dir
                   // 是目标方向（MOTOR_FWD、MOTOR_REV 或 MOTOR_STOP）。
  if (dir == MOTOR_FWD) {
    HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_IN1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_IN2_PIN, GPIO_PIN_RESET);
  } else if (dir == MOTOR_REV) {
    HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_IN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_IN2_PIN, GPIO_PIN_SET);
  } else {
    /* 停止：两个输入都拉低 */
    HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_IN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_IN2_PIN, GPIO_PIN_RESET);
  }
}

void Motor_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {
      0}; // 作用是初始化电机控制的 GPIO 引脚。GPIO_InitTypeDef
          // 是一个结构体类型，用于配置 GPIO
          // 的模式、输出类型、上拉/下拉电阻和速度等参数。

  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* 先把方向引脚拉到低电平，避免上电瞬间电机抖动 */
  HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_IN1_PIN | MOTOR_IN2_PIN, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin =
      MOTOR_IN1_PIN | MOTOR_IN2_PIN; // 作用是设置要初始化的 GPIO
                                     // 引脚，这里是电机控制的 IN1 和 IN2 引脚。
  GPIO_InitStruct.Mode =
      GPIO_MODE_OUTPUT_PP; // 作用是设置 GPIO 的模式为推挽输出模式（Output
                           // Push-Pull），适合驱动电机控制信号。
  GPIO_InitStruct.Pull = GPIO_NOPULL; // 作用是设置 GPIO 的上拉/下拉电阻为无（No
                                      // Pull），即不使用内部上拉或下拉电阻。
  GPIO_InitStruct.Speed =
      GPIO_SPEED_FREQ_HIGH; // 作用是设置 GPIO 的输出速度为高速（High
                            // Speed），以确保电机控制信号的响应速度。
  HAL_GPIO_Init(
      MOTOR_PORT,
      &GPIO_InitStruct); // 作用是调用 HAL 库函数初始化 GPIO，引脚为 MOTOR_PORT
                         // 上的 IN1 和 IN2，引脚配置由 GPIO_InitStruct 指定。

  motor_dir = MOTOR_STOP;
  motor_gear = 0;
  pwm_duty = 0;
  target_pack = 0; /* 方向 STOP、档位 0 */
  pwm_running = 0;
}

void Motor_SetTarget(uint8_t dir,
                     uint8_t gear) // 作用是设置电机的目标方向和档位，参数 dir
                                   // 是目标方向（MOTOR_FWD、MOTOR_REV 或 //
                                   // MOTOR_STOP），gear 是目标档位（0~10）。
{
  if (gear > MOTOR_GEAR_MAX)
    gear = MOTOR_GEAR_MAX;

  /* 一次 16 位写入，TaskMotor 不可能读到更新了一半的组合 */
  target_pack =
      ((uint16_t)dir << 8) |
      (uint16_t)
          gear; // 作用是将目标方向和档位打包成一个 16
                // 位整数，方便在多任务环境下原子更新。高字节存储方向，低字节存储档位。
}

/**
 * @brief  平滑变速状态机
 */
void Motor_SmoothUpdate(void) {
  /* 开头一次性取快照。本函数运行期间别的任务随时可能调 Motor_SetTarget()，
     中途重新读会拿到"新方向 + 旧档位"的混合值。 */
  const uint16_t snap =
      target_pack; // 作用是获取目标方向和档位的快照，避免在函数运行期间被其他任务修改。snap
                   // 是一个 16 位整数，高字节存储目标方向，低字节存储目标档位。
  const uint8_t t_dir = (uint8_t)(snap >> 8);
  const uint8_t t_gear = (uint8_t)(snap & 0xFF);

  /* ---- 情况一：目标为停止，或目标档位为 0 → 平滑减速到 0 后彻底关闭 ---- */
  if (t_dir == MOTOR_STOP || t_gear == 0) {
    if (motor_gear > 0) {
      motor_gear =
          (motor_gear > MOTOR_GEAR_STEP)
              ? (motor_gear - MOTOR_GEAR_STEP)
              : 0; // 作用是平滑减速到
                   // 0，如果当前档位大于步长，则减去步长，否则直接置为 0。

      if (motor_gear == 0) {
        Motor_ApplyDirection(MOTOR_STOP);
        PWM_SafeStop();
        motor_dir = MOTOR_STOP;
        pwm_duty = 0;
      } else {
        PWM_ApplyGear(motor_gear);
      }
    } else if (motor_dir != MOTOR_STOP) {
      /* 档位已经是 0，但方向还没复位，补一次收尾 */
      Motor_ApplyDirection(MOTOR_STOP);
      PWM_SafeStop();
      motor_dir = MOTOR_STOP;
      pwm_duty = 0;
    }
    return;
  }

  /* ---- 目标需要运行：先确保 PWM 已启动 ---- */
  PWM_SafeStart();

  /* ---- 情况二：需要换向 → 先减速到 0，停稳后再翻转方向 ---- */
  if (t_dir != motor_dir && motor_dir != MOTOR_STOP) {
    if (motor_gear > 0) {
      motor_gear =
          (motor_gear > MOTOR_GEAR_STEP) ? (motor_gear - MOTOR_GEAR_STEP) : 0;
      PWM_ApplyGear(motor_gear);
      return;
    }

    /* 已经停稳，可以安全换向；下一个 tick 再开始升速 */
    Motor_ApplyDirection(t_dir);
    motor_dir = t_dir;
    return;
  }

  /* ---- 情况三：从停止状态启动，先建立方向再升速 ---- */
  if (motor_dir == MOTOR_STOP) {
    Motor_ApplyDirection(t_dir);
    motor_dir = t_dir;
  }

  /* ---- 情况四：方向相同 → 按步长向目标档位靠拢 ---- */
  if (motor_gear < t_gear) {
    motor_gear += MOTOR_GEAR_STEP;
    if (motor_gear > t_gear)
      motor_gear = t_gear;
  } else if (motor_gear > t_gear) {
    /* 步长不能超过"还差多少"，否则会回绕。
       motor_gear 是 uint8_t：它比步长还小时（比如 4 - 5），
       减法在 8 位上回绕成 251~255；而原来那句兜底
       if (motor_gear < t_gear) 判不出来（255 < 1 是假），
       于是一路从 250 往下掉 —— 串口就会打出 Speed:230 这种三位数，
       PWM 占空比也跟着乱（255×10 截成 uint8_t 变成 246）。
       本分支进入时必有 motor_gear > t_gear，所以差值 >= 1，
       不会反过来欠减。 */
    uint8_t step = MOTOR_GEAR_STEP;
    if (step > (uint8_t)(motor_gear - t_gear)) {
      step = (uint8_t)(motor_gear - t_gear);
    }
    motor_gear -= step;
  }

  PWM_ApplyGear(motor_gear);
}

uint8_t Motor_GetGear(void) { return motor_gear; }
uint8_t Motor_GetDir(void) { return motor_dir; }
uint8_t Motor_GetDuty(void) { return pwm_duty; }

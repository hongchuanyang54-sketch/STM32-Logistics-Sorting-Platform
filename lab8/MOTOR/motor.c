#include "motor.h"
#include "tim.h"

/* ============================================================
 * 模块私有状态
 *   motor_*  = 当前实际值（平滑过渡过程中会逐档变化）
 *   target_* = 目标值（由 Motor_SetTarget 设定）
 * ============================================================ */
static uint8_t motor_dir   = MOTOR_STOP;   /* 当前实际方向 */
static uint8_t motor_gear  = 0;            /* 当前实际档位 0~10 */
static uint8_t pwm_duty    = 0;            /* 当前占空比 0~100 = 档位 × 10 */

static uint8_t target_dir  = MOTOR_STOP;   /* 目标方向 */
static uint8_t target_gear = 0;            /* 目标档位 */

static uint8_t pwm_running = 0;            /* PWM 是否已启动 */

/**
  * @brief  安全启动 PWM：只在未运行时启动
  * @note   重复调用 HAL_TIM_PWM_Start 会让 HAL 返回错误状态，
  *         因此用 pwm_running 做一次软件开关。
  */
static void PWM_SafeStart(void)
{
    if (!pwm_running)
    {
        HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
        pwm_running = 1;
    }
}

/**
  * @brief  安全停止 PWM：先清零比较值再关闭输出通道
  */
static void PWM_SafeStop(void)
{
    if (pwm_running)
    {
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
static void PWM_ApplyGear(uint8_t gear)
{
    pwm_duty = gear * 10;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (uint32_t)(pwm_duty * 99) / 100);
}

/**
  * @brief  直接设置 IN1/IN2 电平
  */
static void Motor_ApplyDirection(uint8_t dir)
{
    if (dir == MOTOR_FWD)
    {
        HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_IN1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_IN2_PIN, GPIO_PIN_RESET);
    }
    else if (dir == MOTOR_REV)
    {
        HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_IN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_IN2_PIN, GPIO_PIN_SET);
    }
    else
    {
        /* 停止：两个输入都拉低 */
        HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_IN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_IN2_PIN, GPIO_PIN_RESET);
    }
}

void Motor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 先把方向引脚拉到低电平，避免上电瞬间电机抖动 */
    HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_IN1_PIN|MOTOR_IN2_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = MOTOR_IN1_PIN|MOTOR_IN2_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MOTOR_PORT, &GPIO_InitStruct);

    motor_dir   = MOTOR_STOP;
    motor_gear  = 0;
    pwm_duty    = 0;
    target_dir  = MOTOR_STOP;
    target_gear = 0;
    pwm_running = 0;
}

void Motor_SetTarget(uint8_t dir, uint8_t gear)
{
    if (gear > MOTOR_GEAR_MAX) gear = MOTOR_GEAR_MAX;

    target_dir  = dir;
    target_gear = gear;
}

/**
  * @brief  平滑变速状态机
  */
void Motor_SmoothUpdate(void)
{
    /* ---- 情况一：目标为停止，或目标档位为 0 → 平滑减速到 0 后彻底关闭 ---- */
    if (target_dir == MOTOR_STOP || target_gear == 0)
    {
        if (motor_gear > 0)
        {
            motor_gear = (motor_gear > MOTOR_GEAR_STEP) ? (motor_gear - MOTOR_GEAR_STEP) : 0;

            if (motor_gear == 0)
            {
                Motor_ApplyDirection(MOTOR_STOP);
                PWM_SafeStop();
                motor_dir = MOTOR_STOP;
                pwm_duty  = 0;
            }
            else
            {
                PWM_ApplyGear(motor_gear);
            }
        }
        else if (motor_dir != MOTOR_STOP)
        {
            /* 档位已经是 0，但方向还没复位，补一次收尾 */
            Motor_ApplyDirection(MOTOR_STOP);
            PWM_SafeStop();
            motor_dir = MOTOR_STOP;
            pwm_duty  = 0;
        }
        return;
    }

    /* ---- 目标需要运行：先确保 PWM 已启动 ---- */
    PWM_SafeStart();

    /* ---- 情况二：需要换向 → 先减速到 0，停稳后再翻转方向 ---- */
    if (target_dir != motor_dir && motor_dir != MOTOR_STOP)
    {
        if (motor_gear > 0)
        {
            motor_gear = (motor_gear > MOTOR_GEAR_STEP) ? (motor_gear - MOTOR_GEAR_STEP) : 0;
            PWM_ApplyGear(motor_gear);
            return;
        }

        /* 已经停稳，可以安全换向；下一个 tick 再开始升速 */
        Motor_ApplyDirection(target_dir);
        motor_dir = target_dir;
        return;
    }

    /* ---- 情况三：从停止状态启动，先建立方向再升速 ---- */
    if (motor_dir == MOTOR_STOP)
    {
        Motor_ApplyDirection(target_dir);
        motor_dir = target_dir;
    }

    /* ---- 情况四：方向相同 → 按步长向目标档位靠拢 ---- */
    if (motor_gear < target_gear)
    {
        motor_gear += MOTOR_GEAR_STEP;
        if (motor_gear > target_gear) motor_gear = target_gear;
    }
    else if (motor_gear > target_gear)
    {
        motor_gear -= MOTOR_GEAR_STEP;
        if (motor_gear < target_gear) motor_gear = target_gear;
    }

    PWM_ApplyGear(motor_gear);
}

uint8_t Motor_GetGear(void) { return motor_gear; }
uint8_t Motor_GetDir(void)  { return motor_dir;  }
uint8_t Motor_GetDuty(void) { return pwm_duty;   }

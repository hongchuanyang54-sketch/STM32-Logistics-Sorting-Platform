/**
 * @file    sorter_control.c
 * @brief   Sorter motor/servo/actuator control implementation
 * @note    This file contains the hardware-level control for the sorter.
 *          Replace the GPIO toggling code below with your actual
 *          motor driver, stepper motor, and servo PWM control logic.
 *          All timing and pin definitions are in sorter_control.h.
 */
#include "sorter_control.h"
#include "oled.h"

/* External flag from main.c: set by USART1 ISR when a complete JSON frame arrives */
extern volatile uint8_t frame_finish_flag;

/* ================================================================
 * Non-blocking delay: waits up to `ms` milliseconds, but returns
 * early (non-zero) if a new USART1 frame arrives during the wait.
 * Caller should abort the motor sequence when this returns non-zero.
 * ================================================================ */
static uint8_t motor_delay_poll(uint32_t ms)
{
    uint32_t elapsed = 0;
    while (elapsed < ms) {
        if (frame_finish_flag) {
            return 1;  /* New frame pending — abort motor action */
        }
        HAL_Delay(10);
        elapsed += 10;
    }
    return 0;  /* Delay completed normally */
}

/* ================================================================
 * Safely stop all motors and release servo (cleanup on abort)
 * ================================================================ */
static void motor_abort_cleanup(uint16_t motor_pin)
{
    HAL_GPIO_WritePin(MOTOR_PORT, motor_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SERVO_PORT, SERVO_PIN, GPIO_PIN_RESET);
}

/* ================================================================
 * Static helper: map direction enum to motor pin
 * ================================================================ */
static uint16_t Sorter_GetMotorPin(SorterDir_t dir)
{
    switch (dir) {
        case DIR_DOWN:  return MOTOR_DOWN_PIN;
        case DIR_UP:    return MOTOR_UP_PIN;
        case DIR_LEFT:  return MOTOR_LEFT_PIN;
        case DIR_RIGHT: return MOTOR_RIGHT_PIN;
        default:        return 0;
    }
}

/* ================================================================
 * Static helper: get direction name string for display
 * ================================================================ */
static const char *Sorter_GetDirName(SorterDir_t dir)
{
    switch (dir) {
        case DIR_DOWN:  return "Down";
        case DIR_UP:    return "Up";
        case DIR_LEFT:  return "Left";
        case DIR_RIGHT: return "Right";
        default:        return "?";
    }
}

/* ================================================================
 * Initialize motor and servo GPIOs
 * ================================================================ */
void SorterControl_Init(void)
{
    GPIO_InitTypeDef gpio_cfg = {0};

    /* Enable GPIOB clock (for motor/servo pins) */
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Configure motor channel pins as push-pull outputs */
    gpio_cfg.Pin   = MOTOR_DOWN_PIN | MOTOR_UP_PIN
                   | MOTOR_LEFT_PIN | MOTOR_RIGHT_PIN;
    gpio_cfg.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_cfg.Pull  = GPIO_NOPULL;
    gpio_cfg.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(MOTOR_PORT, &gpio_cfg);

    /* Configure servo pin as push-pull output */
    gpio_cfg.Pin   = SERVO_PIN;
    gpio_cfg.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_cfg.Pull  = GPIO_NOPULL;
    gpio_cfg.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SERVO_PORT, &gpio_cfg);

    /* Ensure all outputs start low */
    HAL_GPIO_WritePin(MOTOR_PORT,
        MOTOR_DOWN_PIN | MOTOR_UP_PIN | MOTOR_LEFT_PIN | MOTOR_RIGHT_PIN,
        GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SERVO_PORT, SERVO_PIN, GPIO_PIN_RESET);
}

/* ================================================================
 * Execute a sorting action for the specified direction
 *
 * Hardware control flow (customize for your actual hardware):
 *   1. Activate sorting gate servo  → direct goods to channel
 *   2. Run channel motor/conveyor   → move goods through channel
 *   3. Stop motor                   → wait for next command
 *   4. Release servo                → return gate to neutral
 *
 * REPLACE the GPIO calls below with your real motor/servo code!
 * ================================================================ */
uint8_t SorterControl_Execute(SorterDir_t dir, const char *goods)
{
    uint16_t motor_pin = Sorter_GetMotorPin(dir);
    if (motor_pin == 0) return 0;

    /* ---- Step 1: Activate servo to direct goods ---- */
    /* TODO: Replace with actual servo PWM control */
    HAL_GPIO_WritePin(SERVO_PORT, SERVO_PIN, GPIO_PIN_SET);
    if (motor_delay_poll(200)) {
        motor_abort_cleanup(motor_pin);
        return 1;  /* New frame arrived — abort to refresh OLED */
    }

    /* ---- Step 2: Run channel motor ---- */
    /* TODO: Replace with actual motor driver control */
    HAL_GPIO_WritePin(MOTOR_PORT, motor_pin, GPIO_PIN_SET);
    if (motor_delay_poll(MOTOR_RUN_TIME_MS)) {
        motor_abort_cleanup(motor_pin);
        return 1;
    }

    /* ---- Step 3: Stop motor ---- */
    HAL_GPIO_WritePin(MOTOR_PORT, motor_pin, GPIO_PIN_RESET);

    /* ---- Step 4: Release servo (return to neutral) ---- */
    if (motor_delay_poll(100)) {
        motor_abort_cleanup(motor_pin);
        return 1;
    }
    HAL_GPIO_WritePin(SERVO_PORT, SERVO_PIN, GPIO_PIN_RESET);
    return 0;
}

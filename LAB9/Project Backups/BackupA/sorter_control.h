/**
 * @file    sorter_control.h
 * @brief   Sorter motor/servo/actuator control interface
 * @note    This module provides the hardware abstraction for sorter
 *          direction control based on parsed JSON commands.
 *          Modify the GPIO pin definitions and control logic in
 *          sorter_control.c to match your actual hardware.
 */
#ifndef __SORTER_CONTROL_H
#define __SORTER_CONTROL_H

#include "main.h"
#include <stdint.h>

/* ================================================================
 * Direction enum for sorting channels
 * ================================================================ */
typedef enum {
    DIR_DOWN  = 0,  /* Down channel  */
    DIR_UP    = 1,  /* Up channel    */
    DIR_LEFT  = 2,  /* Left channel  */
    DIR_RIGHT = 3   /* Right channel */
} SorterDir_t;

/* ================================================================
 * Motor/Servo GPIO pin definitions
 * Adjust these based on your actual hardware wiring!
 * ================================================================ */

/* --- Conveyor / channel motor control pins --- */
/* Default: PB0~PB3 as channel enable pins (active high) */
#define MOTOR_PORT          GPIOB
#define MOTOR_DOWN_PIN      GPIO_PIN_0
#define MOTOR_UP_PIN        GPIO_PIN_1
#define MOTOR_LEFT_PIN      GPIO_PIN_2
#define MOTOR_RIGHT_PIN     GPIO_PIN_3

/* --- Sorting gate servo control pin --- */
/* Default: PB4 as servo signal (PWM capable pin for timer output) */
/* If using simple GPIO on/off for servo, this pin is used as digital out */
#define SERVO_PORT          GPIOB
#define SERVO_PIN           GPIO_PIN_4

/* --- Action timing (ms) --- */
/* Adjust these delays based on your mechanical system */
#define MOTOR_RUN_TIME_MS   1500   /* Motor run duration per sort action */
#define SERVO_HOLD_TIME_MS  800    /* Servo gate hold time */

/* ================================================================
 * Function prototypes
 * ================================================================ */

/**
 * @brief  Initialize motor and servo GPIOs
 * @note   Call once during system init before using any control functions
 */
void SorterControl_Init(void);

/**
 * @brief  Execute a sorting action for the specified direction
 * @param  dir   Target direction (DIR_DOWN / DIR_UP / DIR_LEFT / DIR_RIGHT)
 * @param  goods Goods identifier string (e.g. "C,D" or "A"), may be empty
 * @retval 0  Motor action completed normally
 * @retval 1  Aborted early — a new USART1 frame arrived during execution
 * @note   This function activates the corresponding motor channel and
 *         triggers the sorting gate servo. Modify the internal logic
 *         in sorter_control.c to match your actual hardware.
 */
uint8_t SorterControl_Execute(SorterDir_t dir, const char *goods);

#endif /* __SORTER_CONTROL_H */

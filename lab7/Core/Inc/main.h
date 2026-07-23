/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define KEY4_Pin GPIO_PIN_0
#define KEY4_GPIO_Port GPIOA
#define KEY4_EXTI_IRQn EXTI0_IRQn
#define KEY1_Pin GPIO_PIN_1
#define KEY1_GPIO_Port GPIOA
#define KEY1_EXTI_IRQn EXTI1_IRQn
#define KEY2_Pin GPIO_PIN_2
#define KEY2_GPIO_Port GPIOA
#define KEY2_EXTI_IRQn EXTI2_IRQn
#define KEY3_Pin GPIO_PIN_3
#define KEY3_GPIO_Port GPIOA
#define KEY3_EXTI_IRQn EXTI3_IRQn
#define SCL_Pin GPIO_PIN_4
#define SCL_GPIO_Port GPIOA
#define SDA_Pin GPIO_PIN_5
#define SDA_GPIO_Port GPIOA
#define KEY_RE_Pin GPIO_PIN_6
#define KEY_RE_GPIO_Port GPIOA
#define KEY_RE_EXTI_IRQn EXTI9_5_IRQn
#define A_Pin GPIO_PIN_0
#define A_GPIO_Port GPIOB
#define B_Pin GPIO_PIN_1
#define B_GPIO_Port GPIOB
#define C_Pin GPIO_PIN_2
#define C_GPIO_Port GPIOB
#define D_Pin GPIO_PIN_3
#define D_GPIO_Port GPIOB
#define E_Pin GPIO_PIN_4
#define E_GPIO_Port GPIOB
#define F_Pin GPIO_PIN_5
#define F_GPIO_Port GPIOB
#define G_Pin GPIO_PIN_6
#define G_GPIO_Port GPIOB
#define DP_Pin GPIO_PIN_7
#define DP_GPIO_Port GPIOB
#define LCD1_Pin GPIO_PIN_8
#define LCD1_GPIO_Port GPIOB
#define LCD2_Pin GPIO_PIN_9
#define LCD2_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

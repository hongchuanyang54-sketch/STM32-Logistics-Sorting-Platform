#ifndef __KEY_H
#define __KEY_H

#include "main.h"

#define KEY_ADD_PIN    KEY1_Pin
#define KEY_SUB_PIN    KEY2_Pin
#define KEY_RST_PIN    KEY3_Pin
#define KEY_GPIO_PORT  GPIOA

void Key_Init(void);
// ????????,?main??
extern uint8_t count;

#endif

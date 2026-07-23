#ifndef __UART_SEND_H
#define __UART_SEND_H
#include "main.h"
#include "usart.h"
void UART_SendData(uint8_t dat);
void Send_Add(void);
void Send_Sub(void);
void Send_Zero(void);
void Send_EStop(void);
void Send_Reset(void);
#endif

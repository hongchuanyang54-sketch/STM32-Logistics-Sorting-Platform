#ifndef __UART_SEND_H
#define __UART_SEND_H

#include "main.h"
#include "usart.h"

/* 需要上报给上位机的按键事件 */
typedef enum
{
    UART_EVT_NONE = 0,
    UART_EVT_ADD,       /* 上报 "ADD\r\n"   */
    UART_EVT_SUB,       /* 上报 "SUB\r\n"   */
    UART_EVT_ZERO,      /* 上报 "ZERO\r\n"  */
    UART_EVT_ESTOP,     /* 上报 "ESTOP\r\n" */
    UART_EVT_RESET      /* 上报 "RESET\r\n" */
} UartEvent_t;

/**
  * @brief  把事件放入发送队列
  * @param  evt 事件编号
  * @note   可在中断中安全调用（内部关中断保护，只是入队，不做发送）
  *         队列满时直接丢弃该事件，不阻塞、不覆盖旧数据
  */
void Uart_EventPush(UartEvent_t evt);

/**
  * @brief  把队列中的事件逐条发送出去
  * @note   在主循环中周期调用。
  *         HAL_UART_Transmit 是阻塞式发送（最长超时 100ms），
  *         放在主循环执行，避免占用中断处理时间。
  */
void Uart_EventProcess(void);

#endif

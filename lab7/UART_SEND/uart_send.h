#ifndef __UART_SEND_H
#define __UART_SEND_H

#include "main.h"
#include "usart.h"
#include "cmsis_os2.h"

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

/*
 * 下面两个队列由 CubeMX 在 freertos.c 里创建
 * （FREERTOS -> Tasks and Queues -> Queues）：
 *   uartEvtQueue  8  x UartEvent_t   待上报事件
 *   uartRxQueue  32  x uint8_t       收到的串口字节
 */
extern osMessageQueueId_t uartEvtQueueHandle;
extern osMessageQueueId_t uartRxQueueHandle;

/**
  * @brief  把事件投递到上报队列
  * @param  evt 事件编号，UART_EVT_NONE 会被忽略
  * @note   中断与任务里都可以调用同一个函数：
  *         osMessageQueuePut 内部用 __get_IPSR() 判断上下文，
  *         中断里会自动走 xQueueSendToBackFromISR 并触发任务切换。
  *         唯一规则：中断里 timeout 必须为 0（本函数恒传 0）。
  *         队列满时直接丢弃，不阻塞。
  */
void Uart_EventPush(UartEvent_t evt);

/**
  * @brief  把事件真正发到串口（阻塞式发送，最长超时 100ms）
  * @note   只在 TaskUartTx 里调用，保证发送是串行的
  */
void Uart_SendEvent(UartEvent_t evt);

#endif

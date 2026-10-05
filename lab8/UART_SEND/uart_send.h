#ifndef __UART_SEND_H
#define __UART_SEND_H

#include "main.h"

/*
 * 串口发送模块
 *
 * 移植 RTOS 之前，应答报文（命令处理里）和周期上报（UI 里）是两处各自
 * 直接调 HAL_UART_Transmit。拆成任务之后这两处分属不同任务，会同时
 * 往同一个 huart1 里塞字节，输出就乱了。
 *
 * 解决办法：谁也不直接发，统一投递到 uartTxQueue，
 * 由唯一的 TaskUartTx 负责真正写串口。
 */

/* 一条待发报文的最大长度（含结尾 '\0'）。
   最长的固定报文 "OK: REV 10 Gear\r\n" 才 17 字符，32 字节留足余量。 */
#define UART_TX_MSG_MAX  32

/* 队列里传的就是这个结构体。
   CubeMX 生成 freertos.c 时会写 sizeof(UartTxMsg_t)，
   所以这个类型名不能改。 */
typedef struct
{
    char text[UART_TX_MSG_MAX];
} UartTxMsg_t;

/**
  * @brief  把一段文本投递到发送队列（不阻塞、立即返回）
  * @param  text 以 '\0' 结尾的字符串，超长会被截断
  * @note   队列满时直接丢弃，不等待 —— 串口是"尽力而为"的输出通道，
        宁可丢一条上报，也不能让命令处理任务卡在这里。
  */
void Uart_SendText(const char *text);

#endif

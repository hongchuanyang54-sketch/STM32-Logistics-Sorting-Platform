#ifndef __UART_RX_H
#define __UART_RX_H

#include "main.h"

/*
 * 串口收帧模块
 *
 * 下行报文形如：
 *   {"Down":"GoodC,GoodD","Up":"GoodA","Left":"","Right":""}\r\n
 *
 * 帧完整性判定沿用原实现：只有同时见到 '}' 与行结束符 '\n' 才算一帧完整，
 * 避免把半截 JSON 当成完整帧解析。
 */

/* 单帧最大长度，同时也是帧队列元素的大小 */
#define RX_FRAME_MAX  256

/* 帧队列里传的就是这个结构体。
   CubeMX 生成 freertos.c 时会写 sizeof(RxFrame_t)，所以类型名不能改。 */
typedef struct
{
    char text[RX_FRAME_MAX];
} RxFrame_t;

/**
  * @brief  往收帧状态机里喂一个字节
  * @param  b   收到的字节
  * @param  out 凑满一整帧时，帧内容写到这里（调用方提供缓冲）
  * @retval 1 = 凑满了一整帧，out 有效；0 = 还没凑满
  * @note   攒帧状态是本模块私有的，只允许 TaskUartRx 一个任务调用。
  *
  *         原实现在串口中断里干这件事（攒 256 字节 + memcpy 到 frame_buffer）。
  *         现在挪到任务里：中断只负责把字节投进队列、立刻返回，
  *         帧的组装交给任务 —— 中断越短越好。
  */
int UartRx_Feed(uint8_t b, RxFrame_t *out);

#endif

/**
 ******************************************************************************
 * @file    uart_frame.h
 * @brief   串口整帧接收的公共定义（DMA + IDLE 方案）
 *
 * 为什么单独放一个头文件：
 *   UartFrame_t 同时被 main.c（中断回调里填充）和 freertos.c（队列元素类型 +
 *   任务里取出解析）使用。放在 main.h 的 USER CODE 区虽然也能编过，但语义上
 *   独立成一个文件更清楚，而且这个文件不属于 CubeMX 生成范围，重新生成代码
 *   不会被改动。
 *
 * 尺寸依据：
 *   现有协议最长一帧约 20 字符（{"Speed":"10"}\r\n），留到 64 字节余量充足；
 *   接收缓冲 128 字节 = 两帧多，保证 IDLE 处理稍慢时也不会立刻溢出。
 ******************************************************************************
 */
#ifndef __UART_FRAME_H
#define __UART_FRAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* DMA 接收缓冲大小（DMA 直接写入这块内存，必须是连续裸内存） */
#define UART_RX_BUF_SIZE 128

/* 单帧最大内容长度（不含结尾 '\0'） */
#define UART_FRAME_MAX 64

/* 帧队列深度：4 帧足够，单帧 <20 字节时余量很大 */
#define UART_FRAME_QUEUE_DEPTH 4

/**
 * @brief 一帧串口数据
 * @note  len 不含结尾 '\0'；data 由回调负责补 '\0'，因此 data 至少要
 *        UART_FRAME_MAX 字节，其中最后一字节留给结束符。
 *        当前 sizeof(UartFrame_t) = 66（2 字节 len + 64 字节 data）。
 */
typedef struct {
  uint16_t len;
  char data[UART_FRAME_MAX];
} UartFrame_t;

#ifdef __cplusplus
}
#endif

#endif /* __UART_FRAME_H */

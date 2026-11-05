/**
 ******************************************************************************
 * @file    uart_frame.h
 * @brief   串口整帧接收的公共定义
 *
 * 同一套架构支持两种"搬运方式"，靠下面的 UART_RX_USE_DMA 一键切换：
 *
 *   DMA 版：USART1_RX → DMA1_Channel5 → uart_rx_buf，CPU 不参与搬运，
 *           线路空闲时 IDLE 中断触发，HAL 一次性告知整帧长度。
 *   IT  版：每字节触发 RXNE 中断，HAL 把字节写进 uart_rx_buf，
 *           线路空闲时 IDLE 中断触发，同样一次性告知整帧长度。
 *
 * 两者的回调、判帧、队列、任务解析逻辑完全一致，只有"谁在搬字节"不同。
 *
 * 为什么默认用 IT 版：
 *   Proteus 对 STM32 的 USART RX DMA 仿真不完整（DMA 请求常常不生效），
 *   在仿真里 0 才能跑通功能；上真板时改成 1 即可验证 DMA 搬运。
 ******************************************************************************
 */
#ifndef __UART_FRAME_H
#define __UART_FRAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 1 = DMA + IDLE（真板推荐）  0 = 中断 + IDLE（Proteus 仿真可用） */
#define UART_RX_USE_DMA 0

/* 接收缓冲大小。IT/DMA 两版共用：HAL 收到这么多字节也会回调一次 */
#define UART_RX_BUF_SIZE 128

/* 单帧最大内容长度（不含结尾 '\0'）。本工程协议最长约 25 字符 */
#define UART_FRAME_MAX 64

/* 帧队列深度 */
#define UART_FRAME_QUEUE_DEPTH 4

/**
 * @brief 一帧串口数据
 * @note  len 不含结尾 '\0'；data 由调用方补 '\0'，因此 data 最后一字节
 *        留作结束符。当前 sizeof(UartFrame_t) = 66。
 */
typedef struct {
  uint16_t len;
  char data[UART_FRAME_MAX];
} UartFrame_t;

#ifdef __cplusplus
}
#endif

#endif /* __UART_FRAME_H */

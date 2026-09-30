#include "uart_send.h"
#include <string.h>

extern UART_HandleTypeDef huart1;

/* 事件队列深度。环形队列保留一个空位用于判满，实际可存放 7 个事件。
 * 主循环每轮（约 2ms）都会清空队列，而按键最短间隔 200ms，余量充足。 */
#define UART_EVT_QUEUE_SIZE 8

/* 环形队列：中断写入 head，主循环读取 tail */
static volatile UartEvent_t evt_queue[UART_EVT_QUEUE_SIZE];
static volatile uint8_t evt_head = 0; /* 写入位置 */
static volatile uint8_t evt_tail = 0; /* 读取位置 */

/* 各事件对应的上报报文 */
static const char *const EVT_TEXT[] = {
    "",          /* UART_EVT_NONE  占位，不会被发送 */
    "ADD\r\n",   /* UART_EVT_ADD   */
    "SUB\r\n",   /* UART_EVT_SUB   */
    "ZERO\r\n",  /* UART_EVT_ZERO  */
    "ESTOP\r\n", /* UART_EVT_ESTOP */
    "RESET\r\n"  /* UART_EVT_RESET */
};

/**
 * @brief  事件入队（中断 / 主循环均可调用）
 */
void Uart_EventPush(
    UartEvent_t
        evt) // 作用是将一个事件入队到环形队列中，以便在主循环中处理和发送。
{
  uint8_t next; // 作用是计算下一个写入位置，用于判断队列是否已满。
  uint32_t
      primask; // 作用是保存当前中断状态，以便在操作队列时禁用中断，确保操作的原子性。

  if (evt == UART_EVT_NONE)
    return; //

  /* 关中断保护 head/tail 的一致性；先保存原中断状态，退出时原样恢复 */
  primask = __get_PRIMASK();
  __disable_irq();

  next = (uint8_t)((evt_head + 1) % UART_EVT_QUEUE_SIZE);
  if (next !=
      evt_tail) // 如果下一个写入位置不等于读取位置，说明队列未满，可以将事件入队。
  {
    evt_queue[evt_head] = evt;
    evt_head = next;
  }

  __set_PRIMASK(primask);
}

/**
 * @brief  事件出队并发送（仅主循环调用）
 */
void Uart_EventProcess(void) {
  while (evt_tail !=
         evt_head) // 如果读取位置不等于写入位置，说明队列中有事件需要处理。
  {
    UartEvent_t evt = evt_queue[evt_tail];
    evt_tail = (uint8_t)((evt_tail + 1) % UART_EVT_QUEUE_SIZE);

    if (evt > UART_EVT_NONE && evt <= UART_EVT_RESET) {
      const char *text = EVT_TEXT[evt];
      HAL_UART_Transmit(&huart1, (uint8_t *)text, strlen(text), 100);
    }
  }
}

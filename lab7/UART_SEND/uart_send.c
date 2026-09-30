#include "uart_send.h"
#include <string.h>

extern UART_HandleTypeDef huart1;

/* 事件队列深度。环形队列保留一个空位用于判满，实际可存放 7 个事件。
 * 主循环每轮（约 2ms）都会清空队列，而按键最短间隔 200ms，余量充足。 */
#define UART_EVT_QUEUE_SIZE  8

/* 环形队列：中断写入 head，主循环读取 tail */
static volatile UartEvent_t evt_queue[UART_EVT_QUEUE_SIZE];
static volatile uint8_t     evt_head = 0;   /* 写入位置 */
static volatile uint8_t     evt_tail = 0;   /* 读取位置 */

/* 各事件对应的上报报文 */
static const char *const EVT_TEXT[] =
{
    "",              /* UART_EVT_NONE  占位，不会被发送 */
    "ADD\r\n",       /* UART_EVT_ADD   */
    "SUB\r\n",       /* UART_EVT_SUB   */
    "ZERO\r\n",      /* UART_EVT_ZERO  */
    "ESTOP\r\n",     /* UART_EVT_ESTOP */
    "RESET\r\n"      /* UART_EVT_RESET */
};

/**
  * @brief  事件入队（中断 / 主循环均可调用）
  */
void Uart_EventPush(UartEvent_t evt)
{
    uint8_t  next;
    uint32_t primask;

    if (evt == UART_EVT_NONE) return;

    /* 关中断保护 head/tail 的一致性；先保存原中断状态，退出时原样恢复 */
    primask = __get_PRIMASK();
    __disable_irq();

    next = (uint8_t)((evt_head + 1) % UART_EVT_QUEUE_SIZE);
    if (next != evt_tail)              /* 队列未满才写入 */
    {
        evt_queue[evt_head] = evt;
        evt_head = next;
    }

    __set_PRIMASK(primask);
}

/**
  * @brief  事件出队并发送（仅主循环调用）
  */
void Uart_EventProcess(void)
{
    while (evt_tail != evt_head)
    {
        UartEvent_t evt = evt_queue[evt_tail];
        evt_tail = (uint8_t)((evt_tail + 1) % UART_EVT_QUEUE_SIZE);

        if (evt > UART_EVT_NONE && evt <= UART_EVT_RESET)
        {
            const char *text = EVT_TEXT[evt];
            HAL_UART_Transmit(&huart1, (uint8_t *)text, strlen(text), 100);
        }
    }
}

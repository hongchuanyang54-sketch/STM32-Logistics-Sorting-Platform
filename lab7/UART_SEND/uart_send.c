#include "uart_send.h"
#include <string.h>

extern UART_HandleTypeDef huart1;

/* 各事件对应的上报报文。数组下标就是 UartEvent_t 的取值 */
static const char *const EVT_TEXT[] =
{
    "",              /* UART_EVT_NONE  占位，不会被发送 */
    "ADD\r\n",       /* UART_EVT_ADD   */
    "SUB\r\n",       /* UART_EVT_SUB   */
    "ZERO\r\n",      /* UART_EVT_ZERO  */
    "ESTOP\r\n",     /* UART_EVT_ESTOP */
    "RESET\r\n"      /* UART_EVT_RESET */
};

void Uart_EventPush(UartEvent_t evt)//该函数的作用是将一个串口事件（如按键事件）投递到上报队列中，以便后续处理和发送到串口。
{
    if (evt == UART_EVT_NONE) return;

    /* 与裸机版的区别：这里不再自己管环形缓冲的 head/tail，
       也不再用 __disable_irq() 关全局中断做保护。
       队列是内核对象，入队是原子的，该保护的内核自己做了。
       —— 这一条替换掉了原来 uart_send.c 里约 30 行手写同步代码。 */
    (void)osMessageQueuePut(uartEvtQueueHandle, &evt, 0U, 0U);
}

void Uart_SendEvent(UartEvent_t evt)
{
    if (evt > UART_EVT_NONE && evt <= UART_EVT_RESET)
    {
        const char *text = EVT_TEXT[evt];
        HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 100);
    }
}

#include "uart_send.h"
#include "cmsis_os2.h"
#include <string.h>

/* 队列由 CubeMX 在 freertos.c 里创建
   （FREERTOS -> Tasks and Queues -> Queues） */
extern osMessageQueueId_t uartTxQueueHandle;

void Uart_SendText(const char *text) {
  UartTxMsg_t msg;
  size_t len;

  if (text == NULL)
    return;

  len = strlen(text);
  if (len >= sizeof(msg.text)) {
    len = sizeof(msg.text) - 1; /* 超长截断，保证 '\0' 有位可放 */
  }

  memcpy(msg.text, text,
         len); // memcpy() 函数将 text 中的 len 个字节复制到 msg.text
               // 中，确保待发送的文本被正确存储在消息结构体中。
  msg.text[len] = '\0'; //  确保消息结构体中的文本以 '\0' 结尾，便于后续处理。

  (void)osMessageQueuePut(
      uartTxQueueHandle, &msg, 0U,
      0U); // 将消息结构体投递到 uartTxQueue 队列中，等待 TaskUartTx
           // 处理发送。参数 0U 表示优先级为默认值，最后一个 0U
           // 表示不阻塞，如果队列满则直接丢弃消息。
}

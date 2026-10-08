#include "uart_rx.h"
#include <string.h>

/* 攒帧状态。只有 TaskUartRx 会碰，所以不需要加锁。 */
static char     s_buf[RX_FRAME_MAX];
static uint16_t s_idx       = 0;    /* s_buf 的写入位置 */
static uint8_t  s_saw_brace = 0;    /* 本帧是否已见到 '}' */

int UartRx_Feed(uint8_t b, RxFrame_t *out)
{
    /* ---- 行结束：'\n' 才可能成帧 ---- */
    if (b == '\n')
    {
        int complete = (s_idx > 0 && s_saw_brace);

        if (complete)
        {
            s_buf[s_idx] = '\0';
            memcpy(out->text, s_buf, (size_t)s_idx + 1);
        }

        s_idx       = 0;    /* CRLF 里跟在 '\r' 后面的 '\n' 也会走到这里，正常收尾 */
        s_saw_brace = 0;
        return complete;
    }

    /* ---- '\r' 直接跳过，兼容 CRLF 与单独 LF 两种行尾 ---- */
    if (b == '\r')
    {
        return 0;
    }

    if (s_idx < RX_FRAME_MAX - 1)
    {
        s_buf[s_idx++] = (char)b;
        if (b == '}')
        {
            s_saw_brace = 1;
        }
    }
    else
    {
        /* 缓冲写满仍未成帧 —— 整帧丢弃，从头再攒，避免溢出 */
        s_idx       = 0;
        s_saw_brace = 0;
    }

    return 0;
}

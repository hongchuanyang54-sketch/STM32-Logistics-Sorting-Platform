#include "key.h"
#include "uart_send.h"

/* ============================================================
 * 中断与主循环共享的状态 —— 必须 volatile，
 * 否则编译器优化后主循环可能一直读到寄存器里的旧值。
 * ============================================================ */
volatile uint8_t  key_press_flag = 0;   /* 0=无 1=加 2=减 3=清零（中断写入） */
volatile uint32_t key_press_tick = 0;   /* 按下时刻 HAL_GetTick()（中断写入） */

/* ============================================================
 * 仅主循环访问，无需 volatile
 * ============================================================ */
static uint32_t key_long_tick   = 0;    /* 上次长按重复触发的时刻 */
static uint8_t  key_single_flag = 0;    /* 本次按下是否已执行过"短按"动作 */

/**
  * @brief  EXTI 中断回调：按键消抖 + 记录按下时刻
  * @note   本函数在中断上下文中执行，只做"记状态"，
  *         真正的加减清零与串口上报都交给主循环（Key_Process + Uart_EventProcess），
  *         避免在中断里调用阻塞式的 HAL_UART_Transmit。
  *         KEY4/KEY_RE 是单次触发事件，没有长短按状态，直接入队上报。
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    static uint32_t last_tick[3]    = {0, 0, 0};
    static uint32_t last_estop_tick = 0;//这是一个静态变量，用于记录上次急停按键触发的时间戳，防止按键抖动导致的误触发
    static uint32_t last_reset_tick = 0;//这是一个静态变量，用于记录上次复位按键触发的时间戳，防止按键抖动导致的误触发

    uint32_t now = HAL_GetTick();//获取当前系统时间戳（毫秒）
    uint8_t  idx = 255;//按键索引，初始值为 255因为 255 不对应任何按键，表示未检测到有效按键，用来区分 KEY1/KEY2/KEY3 的按键索引

    /* ---- KEY4 (PA0) 急停：独立处理，不参与计数 ---- */
    if (GPIO_Pin == KEY4_Pin)
    {
        if (now - last_estop_tick < KEY_DEBOUNCE_MS) return;//判断消抖
        last_estop_tick = now;
        Uart_EventPush(UART_EVT_ESTOP);//将急停事件推入串口事件队列，等待主循环处理
        return;
    }

    /* ---- KEY_RE (PA6) 急停复位：独立处理 ---- */
    if (GPIO_Pin == KEY_RE_Pin)
    {
        if (now - last_reset_tick < KEY_DEBOUNCE_MS) return;//判断消抖
        last_reset_tick = now;
        Uart_EventPush(UART_EVT_RESET);//将复位事件推入串口事件队列，等待主循环处理
        return;
    }

    /* ---- KEY1/KEY2/KEY3 计数按键 ---- */
    if      (GPIO_Pin == KEY1_Pin) idx = 0;//按键索引 0 对应 KEY1
    else if (GPIO_Pin == KEY2_Pin) idx = 1;//按键索引 1 对应 KEY2
    else if (GPIO_Pin == KEY3_Pin) idx = 2;//按键索引 2 对应 KEY3
    else return;
    if (idx == 255) return;

    /* 按键消抖 */
    if (now - last_tick[idx] < KEY_DEBOUNCE_MS) return;
    last_tick[idx] = now;

    /* 记录按下；长短按的区分与连发由主循环的 Key_Process() 负责 */
    key_press_flag = idx + 1;
    key_press_tick = now;
}

/**
  * @brief  按键状态机（在主循环中周期调用）
  * @retval 本次需要执行的动作
  */
KeyEvent_t Key_Process(void)
{
    uint32_t now;

    /* ---- 1. 松手检测：引脚回到低电平，结束本次按键 ---- */
    if ((key_press_flag == 1 && HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY_ADD_PIN) == GPIO_PIN_RESET) ||
        (key_press_flag == 2 && HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY_SUB_PIN) == GPIO_PIN_RESET) ||
        (key_press_flag == 3 && HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY_RST_PIN) == GPIO_PIN_RESET))//判断按键是否松开
    {
        key_press_flag  = 0;
        key_single_flag = 0;
    }

    if (key_press_flag == 0) return KEY_EVENT_NONE;

    now = HAL_GetTick();

    /* ---- 2. 短按：按下后 KEY_DEBOUNCE_MS 内触发一次 ---- */
    if (now - key_press_tick < KEY_DEBOUNCE_MS)
    {
        if (key_single_flag == 0)
        {
            key_single_flag = 1;
            key_long_tick   = now;

            switch (key_press_flag)
            {
                case 1: return KEY_EVENT_ADD;
                case 2: return KEY_EVENT_SUB;

                case 3:
                    /* 清零是一次性动作，本次按键到此结束 */
                    key_press_flag  = 0;
                    key_single_flag = 0;
                    return KEY_EVENT_CLEAR;

                default: break;
            }
        }
        return KEY_EVENT_NONE;
    }

    /* ---- 3. 长按：每 KEY_REPEAT_MS 重复触发一次（仅加/减支持连发） ---- */
    if (now - key_long_tick >= KEY_REPEAT_MS)
    {
        key_long_tick = now;

        switch (key_press_flag)
        {
            case 1: return KEY_EVENT_ADD;
            case 2: return KEY_EVENT_SUB;
            default: break;
        }
    }

    return KEY_EVENT_NONE;
}

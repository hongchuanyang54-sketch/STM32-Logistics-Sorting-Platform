#ifndef __KEY_H
#define __KEY_H

#include "main.h"

/*
 * 按键引脚（KEY1~KEY4 / KEY_RE 的 User Label 定义在 CubeMX 生成的 main.h 中）
 *
 * 引脚本身由 MX_GPIO_Init() 统一配置：
 *   PA0(KEY4) / PA1(KEY1) / PA2(KEY2) / PA3(KEY3) / PA6(KEY_RE)
 *   全部为「上升沿触发 + 内部下拉」，即按下 = 高电平。
 * 中断使能与优先级（EXTI0/1/2/3/9_5，抢占优先级 0）同样在 MX_GPIO_Init() 中完成。
 */
#define KEY_ADD_PIN    KEY1_Pin      /* PA1：加   */
#define KEY_SUB_PIN    KEY2_Pin      /* PA2：减   */
#define KEY_RST_PIN    KEY3_Pin      /* PA3：清零 */
#define KEY_GPIO_PORT  GPIOA

/* 消抖窗口，同时用作"短按"的判定门限（ms） */
#define KEY_DEBOUNCE_MS  200

/* 长按后连加/连减的重复周期（ms）—— 约 5 次/秒 */
#define KEY_REPEAT_MS    200

/* 主循环取用的按键动作 */
typedef enum
{
    KEY_EVENT_NONE = 0,
    KEY_EVENT_ADD,      /* 加一 */
    KEY_EVENT_SUB,      /* 减一 */
    KEY_EVENT_CLEAR     /* 清零 */
} KeyEvent_t;

/**
  * @brief  按键状态机，在主循环中周期调用
  * @retval 本次需要执行的动作，无动作时返回 KEY_EVENT_NONE
  * @note   短按：按下后 KEY_DEBOUNCE_MS 内触发一次
  *         长按：按住不放，每 KEY_REPEAT_MS 重复触发一次（仅加/减支持连发）
  *         松手检测：发现引脚回到低电平即结束本次按键
  */
KeyEvent_t Key_Process(void);

#endif

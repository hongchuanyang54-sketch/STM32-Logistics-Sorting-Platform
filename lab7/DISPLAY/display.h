#ifndef __DISPLAY_H
#define __DISPLAY_H

#include "main.h"

/*
 * 2 位共阳数码管驱动
 *   段选：PB0~PB7 -> A B C D E F G DP   （低电平点亮）
 *   位选：PB8 = 十位、PB9 = 个位         （高电平选中，经 NPN 三极管给公共阳极供电）
 *
 * 注意：两位数码管共用同一组段选线，同一时刻只能点亮一位，
 *       必须周期性反复调用 Display_ShowNum() 才能看到"两位同时亮"。
 */

void Display_Init(void);

/**
  * @brief  显示 0~99
  * @param  num 待显示数值，超出 99 时只显示后两位
  * @note   一次调用完成一轮扫描（十位 1ms + 个位 1ms），需在主循环中反复调用
  */
void Display_ShowNum(uint8_t num);

#endif

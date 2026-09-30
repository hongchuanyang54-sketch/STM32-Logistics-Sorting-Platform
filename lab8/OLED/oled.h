#ifndef __OLED_H
#define __OLED_H

#include "main.h"

/*
 * SSD1306 128x64 OLED，走软件 I²C（见 SOFT_I2C/soft_i2c.c）
 *
 * 字符使用 6x8 点阵字库（F6x8，只含 ASCII 32~127），
 * 坐标 x 以像素为单位（每个字符占 6 列），y 以页为单位（每页 8 像素，共 8 页）。
 */

/* 从机地址：7 位地址 0x3C 左移 1 位后的 8 位形式 */
#define OLED_ADDR  0x78

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t x, uint8_t y, char ch);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len);

#endif

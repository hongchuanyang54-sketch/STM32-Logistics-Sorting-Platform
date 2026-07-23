#ifndef __OLED_H
#define __H
#include "main.h"
#include "soft_i2c.h"

#define OLED_ADDR 0x3C

// ?????
typedef enum
{
    CHAR_COLON,    // 0 :
    CHAR_X,        // 1 X
    CHAR_Y,        // 2 Y
    CHAR_COMMA,    // 3 ,
    CHAR_DOT,      // 4 .
    CHAR_LBRACE,   // 5 {
    CHAR_RBRACE,   // 6 }
    CHAR_O,        // 7 O
    CHAR_K,        // 8 K
    CHAR_E,        // 9 E
    CHAR_R,        // 10 R
    CHAR_W,        // 11 W
    CHAR_A,        // 12 A
    CHAR_I,        // 13 I
    CHAR_T,        // 14 T
    CHAR_G,        // 15 G
    CHAR_D,        // 16 D
    CHAR_S,        // 17 S
    CHAR_N,        // 18 N
    CHAR_0,        // 19 0
    CHAR_1,        // 20 1
    CHAR_2,        // 21 2
    CHAR_3,        // 22 3
    CHAR_4,        // 23 4
    CHAR_5,        // 24 5
    CHAR_6,        // 25 6
    CHAR_7,        // 26 7
    CHAR_8,        // 27 8
    CHAR_9,        // 28 9
    CHAR_B,        // 29 B
    CHAR_C         // 30 C
} CharIndex;

extern const unsigned char FontMini[][16];
extern const unsigned char FontChinese[4][32];

void OLED_Init(void);
void OLED_Clear(void);
void OLED_WriteCmd(uint8_t cmd);
void OLED_WriteData(uint8_t dat);
void OLED_SetPos(uint8_t x, uint8_t y);

void OLED_ShowChar(uint8_t x,uint8_t y,uint8_t char_idx);
void OLED_ShowMiniStr(uint8_t x,uint8_t y,char *str);
void OLED_ShowChinese(uint8_t x,uint8_t y,uint8_t num);
void OLED_ShowString(uint8_t x,uint8_t y,char *str);
#endif

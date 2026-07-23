#ifndef __OLED_FONT_H
#define __OLED_FONT_H

#include "main.h"
#include <stdint.h>

// ===== ?????? =====
extern const uint8_t font_qian[32];     // ?
extern const uint8_t font_ru[32];       // ?
extern const uint8_t font_shi[32];      // ?
extern const uint8_t font_xi[32];       // ?
extern const uint8_t font_tong[32];     // ?

// ===== ?????? =====
extern const uint8_t font_huowu[32];    // ??
extern const uint8_t font_shuliang[32]; // ??
extern const uint8_t font_zhuangtai[32];// ??

// ===== ???? =====
extern const uint8_t font_pingguo[32];  // ??
extern const uint8_t font_xiangjiao[32];// ??
extern const uint8_t font_chengzi[32];  // ??
extern const uint8_t font_putao[32];    // ??
extern const uint8_t font_xigua[32];    // ??

// ===== ????(16x16)=====
extern const uint8_t num_16x16[10][32];

// ===== ????? =====
typedef struct {
    uint8_t id;
    const char *name;
    const uint8_t *font;
} GoodsInfo_t;

extern const GoodsInfo_t GoodsTable[];
extern const uint8_t GOODS_COUNT;

#endif

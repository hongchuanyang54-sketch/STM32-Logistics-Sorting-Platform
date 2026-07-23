#ifndef __SORTER_LOGIC_H
#define __SORTER_LOGIC_H

#include <stdint.h>

// ????
typedef enum {
    STATE_IDLE = 0,
    STATE_TERMINAL_MODE,
    STATE_APRILTAG_MODE,
    STATE_DISPLAY_UPDATE
} Sorter_State_t;

// ?????
typedef struct {
    Sorter_State_t state;
    uint8_t current_goods_id;
    uint32_t goods_count[10];
    uint32_t total_count;
} Sorter_Context_t;

// ????
void Sorter_Init(void);
void Sorter_Process(void);
void Sorter_SetGoods(uint8_t goods_id);
void Sorter_DisplayStatus(void);
uint8_t Sorter_GetCurrentGoods(void);
uint32_t Sorter_GetCount(uint8_t goods_id);
uint32_t Sorter_GetTotal(void);

#endif

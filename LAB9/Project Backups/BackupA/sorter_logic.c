#include "sorter_logic.h"
#include "oled.h"
#include "oled_font.h"
#include "uart_recv.h"
#include "json_parser.h"
#include <string.h>
#include <stdio.h>

// ===== ???? =====
extern const GoodsInfo_t GoodsTable[];
extern const uint8_t GOODS_COUNT;
extern const uint8_t font_qian[32];
extern const uint8_t font_ru[32];
extern const uint8_t font_shi[32];
extern const uint8_t font_xi[32];
extern const uint8_t font_tong[32];

// ===== ???? =====
static Sorter_Context_t g_sorter;

// ===== ???? =====

void Sorter_Init(void) {
    g_sorter.state = STATE_IDLE;
    g_sorter.current_goods_id = 0;
    g_sorter.total_count = 0;
    for (int i = 0; i < 10; i++) {
        g_sorter.goods_count[i] = 0;
    }
    
    // ???? "??????"
    OLED_Clear();
    OLED_ShowChinese(0, 0, font_qian);
    OLED_ShowChinese(16, 0, font_ru);
    OLED_ShowChinese(32, 0, font_shi);
    OLED_ShowChinese(48, 0, font_xi);
    OLED_ShowChinese(64, 0, font_tong);
    
    // ?????
    OLED_ShowString(0, 4, "??:--  ??:0");
    OLED_ShowString(0, 6, "??:0");
}

void Sorter_Process(void) {
    if (!uart_frame_ready) return;
    
    uint8_t goods_id = 0;
    
    if (uart_mode == MODE_TERMINAL) {
        // ???:?? "VAL:XX" ??
        goods_id = UART_GetGoodsIdFromTerminal();
        if (goods_id >= 1 && goods_id <= GOODS_COUNT) {
            Sorter_SetGoods(goods_id);
        }
    }
    else if (uart_mode == MODE_APRILTAG) {
        // ???:??JSON
        AprilTag_Result_t tag;
        if (ParseAprilTagJSON(json_buffer, &tag) && tag.valid) {
            if (tag.tag_id >= 1 && tag.tag_id <= GOODS_COUNT) {
                Sorter_SetGoods(tag.tag_id);
            }
        }
    }
    
    uart_frame_ready = 0;
}

void Sorter_SetGoods(uint8_t goods_id) {
    if (goods_id < 1 || goods_id > GOODS_COUNT) return;
    
    g_sorter.current_goods_id = goods_id;
    g_sorter.goods_count[goods_id - 1]++;
    g_sorter.total_count++;
    
    // ????
    Sorter_DisplayStatus();
}

void Sorter_DisplayStatus(void) {
    char buf[16];
    uint8_t id = g_sorter.current_goods_id;
    
    // ??????(?4?)
    OLED_SetPos(32, 4);  // ?"??:"????
    if (id >= 1 && id <= GOODS_COUNT) {
        OLED_ShowChinese(32, 4, GoodsTable[id - 1].font);
    } else {
        OLED_ShowString(32, 4, "--");
    }
    
    // ????(?5?)
    if (id >= 1 && id <= GOODS_COUNT) {
        uint32_t count = g_sorter.goods_count[id - 1];
        uint8_t ten = count / 10;
        uint8_t one = count % 10;
        OLED_SetPos(56, 5);
        OLED_ShowNum(56, 5, ten * 10 + one);
    }
    
    // ????(?7?)
    uint8_t total_ten = g_sorter.total_count / 10;
    uint8_t total_one = g_sorter.total_count % 10;
    OLED_SetPos(32, 6);  // "??:"??
    OLED_ShowNum(32, 6, total_ten * 10 + total_one);
}

uint8_t Sorter_GetCurrentGoods(void) {
    return g_sorter.current_goods_id;
}

uint32_t Sorter_GetCount(uint8_t goods_id) {
    if (goods_id < 1 || goods_id > 10) return 0;
    return g_sorter.goods_count[goods_id - 1];
}

uint32_t Sorter_GetTotal(void) {
    return g_sorter.total_count;
}

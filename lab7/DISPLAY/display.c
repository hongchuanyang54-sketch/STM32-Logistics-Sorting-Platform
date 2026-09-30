#include "display.h"

/* ================= 引脚定义 ================= */
#define SEG_PORT    GPIOB
#define SEG_MASK    0x00FFu          /* PB0~PB7 = A~G + DP */

#define DIGIT1_PORT GPIOB            /* 十位 */
#define DIGIT1_PIN  GPIO_PIN_8

#define DIGIT2_PORT GPIOB            /* 个位 */
#define DIGIT2_PIN  GPIO_PIN_9

/* 每位点亮保持时间（ms） */
#define DIGIT_ON_MS 1

/* 共阳数码管段码表：数字 0~9，低电平点亮。0xC0 即 "0" */
const uint8_t SEG_TABLE[10] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90
};

/**
  * @brief  初始化数码管 IO
  * @note   段选 PB0~PB7、位选 PB8/PB9 均为推挽输出
  */
void Display_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB0~PB7 段选 */
    GPIO_InitStruct.Pin   = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|
                            GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SEG_PORT, &GPIO_InitStruct);

    /* PB8/PB9 位选 */
    GPIO_InitStruct.Pin = DIGIT1_PIN|DIGIT2_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* 上电先熄灭两位，避免显示残影 */
    HAL_GPIO_WritePin(DIGIT1_PORT, DIGIT1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DIGIT2_PORT, DIGIT2_PIN, GPIO_PIN_RESET);
}

/**
  * @brief  把 8 位段码写到 PB0~PB7
  * @note   直接改 ODR，只覆盖低 8 位，不影响 PB8/PB9 的位选状态
  */
static void SEG_Write(uint8_t dat)
{
    SEG_PORT->ODR = (SEG_PORT->ODR & ~SEG_MASK) | dat;
}

/**
  * @brief  显示 0~99
  * @note   动态扫描：先送十位段码并点亮十位 1ms，再送个位段码并点亮个位 1ms。
  *         靠人眼视觉暂留看起来才是"两位同时亮"。
  *         因此本函数必须被周期性反复调用（当前是在主循环里每轮调用一次）。
  */
void Display_ShowNum(uint8_t num)
{
    uint8_t ten = num / 10;
    uint8_t one = num % 10;

    /* ---- 十位 ---- */
    SEG_Write(SEG_TABLE[ten]);
    HAL_GPIO_WritePin(DIGIT1_PORT, DIGIT1_PIN, GPIO_PIN_SET);
    HAL_Delay(DIGIT_ON_MS);
    HAL_GPIO_WritePin(DIGIT1_PORT, DIGIT1_PIN, GPIO_PIN_RESET);

    /* ---- 个位 ---- */
    SEG_Write(SEG_TABLE[one]);
    HAL_GPIO_WritePin(DIGIT2_PORT, DIGIT2_PIN, GPIO_PIN_SET);
    HAL_Delay(DIGIT_ON_MS);
    HAL_GPIO_WritePin(DIGIT2_PORT, DIGIT2_PIN, GPIO_PIN_RESET);
}

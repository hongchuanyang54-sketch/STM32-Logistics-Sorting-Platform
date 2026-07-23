#include "main.h"

/* ========== ????????? ========== */
#define SEG_PORT    GPIOB
#define SEG_MASK    0x00FFu  // PB0~PB7 ??
#define DIGIT1_PORT GPIOB
#define DIGIT1_PIN  GPIO_PIN_8 // ????
#define DIGIT2_PORT GPIOB
#define DIGIT2_PIN  GPIO_PIN_9 // ????

// ??????? 0~9
const uint8_t SEG_TABLE[] = {
    0xC0, 0xF9, 0xA4, 0xB0,
    0x99, 0x92, 0x82, 0xF8,
    0x80, 0x90
};

/**
  * @brief ???IO???
  */
void Display_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // PB0~PB7 ???
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|
                          GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SEG_PORT, &GPIO_InitStruct);

    // PB8 PB9 ????
    GPIO_InitStruct.Pin = DIGIT1_PIN|DIGIT2_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // ?????
    HAL_GPIO_WritePin(DIGIT1_PORT, DIGIT1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DIGIT2_PORT, DIGIT2_PIN, GPIO_PIN_RESET);
}

/**
  * @brief ???????(???????,??????)
  */
static void SEG_Write(uint8_t dat)
{
    SEG_PORT->ODR = (SEG_PORT->ODR & ~SEG_MASK) | dat;
}

/**
  * @brief ????0~99????
  */
void Display_ShowNum(uint8_t num)
{
    uint8_t ten = num / 10;
    uint8_t one = num % 10;

    // ????
    SEG_Write(SEG_TABLE[ten]);
    HAL_GPIO_WritePin(DIGIT1_PORT, DIGIT1_PIN, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(DIGIT1_PORT, DIGIT1_PIN, GPIO_PIN_RESET);

    // ????
    SEG_Write(SEG_TABLE[one]);
    HAL_GPIO_WritePin(DIGIT2_PORT, DIGIT2_PIN, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(DIGIT2_PORT, DIGIT2_PIN, GPIO_PIN_RESET);
}


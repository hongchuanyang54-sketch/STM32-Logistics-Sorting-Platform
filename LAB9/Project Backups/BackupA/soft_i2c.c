/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    : soft_i2c.c
  * @brief   : Software I2C Driver ??????Proteus
  ******************************************************************************
  */
#include "soft_i2c.h"

void SoftI2C_Delay(void) {
    uint32_t i;
    for (i = 0; i <5; i++);
}

void SoftI2C_Init(void) {
    GPIO_InitTypeDef gpio_cfg = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio_cfg.Pin = SCL_PIN | SDA_PIN;
    gpio_cfg.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_cfg.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_cfg.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(I2C_PORT, &gpio_cfg);
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN | SDA_PIN, GPIO_PIN_SET);
}

void SoftI2C_Start(void) {
    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_SET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_RESET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_RESET);
}

void SoftI2C_Stop(void) {
    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_SET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_SET);
    SoftI2C_Delay();
}

void SoftI2C_WriteByte(uint8_t dat) {
    uint8_t i;
    for (i = 0; i < 8; i++) {
        if (dat & 0x80) {
            HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_RESET);
        }
        dat <<= 1;
        SoftI2C_Delay();
        HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_SET);
        SoftI2C_Delay();
        HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_RESET);
    }
    SoftI2C_Delay();
    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_SET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_SET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_RESET);
}

uint8_t SoftI2C_ReadByte(uint8_t ack) {
    uint8_t i, dat = 0;
    GPIO_InitTypeDef gpio_cfg = {0};
    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_SET);
    gpio_cfg.Pin = SDA_PIN;
    gpio_cfg.Mode = GPIO_MODE_INPUT;
    gpio_cfg.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(I2C_PORT, &gpio_cfg);
    for (i = 0; i < 8; i++) {
        dat <<= 1;
        HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_SET);
        SoftI2C_Delay();
        if (HAL_GPIO_ReadPin(I2C_PORT, SDA_PIN)) dat |= 0x01;
        HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_RESET);
        SoftI2C_Delay();
    }
    gpio_cfg.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_cfg.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(I2C_PORT, &gpio_cfg);
    if (ack) {
        HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_SET);
    }
    SoftI2C_Delay();
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_SET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_RESET);
    return dat;
}

/**
 * @brief  Send multiple data bytes in a single I2C transaction.
 *         Caller wraps with START(address+W) + 0x40 + bulk_data + STOP.
 *         Each byte still respects the SSD1306 ACK timing.
 */
void SoftI2C_WriteBulk(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        SoftI2C_WriteByte(data[i]);
    }
}

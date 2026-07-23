#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H
#include "main.h"

#define SCL_PIN GPIO_PIN_6
#define SDA_PIN GPIO_PIN_7
#define I2C_PORT GPIOA

void SoftI2C_Delay(void);
void SoftI2C_Init(void);
void SoftI2C_Start(void);
void SoftI2C_Stop(void);
void SoftI2C_WriteByte(uint8_t dat);
uint8_t SoftI2C_ReadByte(uint8_t ack);

/**
 * @brief  Send multiple data bytes in a single I2C transaction.
 * @note   Caller must send START + address + 0x40 before calling,
 *         and STOP after.  This skips the per-byte START/STOP
 *         overhead and is 5-6x faster than individual OLED_WriteData.
 */
void SoftI2C_WriteBulk(const uint8_t *data, uint16_t len);

#endif

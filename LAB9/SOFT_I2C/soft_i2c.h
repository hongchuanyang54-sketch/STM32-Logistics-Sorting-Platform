#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H

#include "main.h"

/*
 * 软件模拟 I²C（位翻转），本项目仅用于驱动 OLED
 *   SCL = PA6，SDA = PA7
 *
 * 两个引脚由 SoftI2C_Init() 配置为「开漏输出 + 内部上拉」，
 * 主机拉高即释放总线，因此可以直接读回电平做 ACK 检测。
 */
#define SCL_PIN   GPIO_PIN_6
#define SDA_PIN   GPIO_PIN_7
#define I2C_PORT  GPIOA

void    SoftI2C_Init(void);
void    SoftI2C_Start(void);
void    SoftI2C_Stop(void);
void    SoftI2C_WriteByte(uint8_t dat);

/**
  * @brief  在一次 START/STOP 事务里连续写出多个字节
  * @param  data 数据指针
  * @param  len  字节数
  * @note   调用方需要自己先发 START + 从机地址 + 控制字节，
  *         全部写完后自己发 STOP。
  *         相比逐字节各发一次 START/STOP，能省掉大量总线开销，
  *         写整屏显存时尤其明显。
  */
void    SoftI2C_WriteBulk(const uint8_t *data, uint16_t len);

#endif

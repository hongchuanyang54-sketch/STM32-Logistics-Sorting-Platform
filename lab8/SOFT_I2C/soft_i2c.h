#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H

#include "main.h"

/*
 * 软件模拟 I²C（位翻转），本项目仅用于驱动 OLED
 *   SCL = PA3，SDA = PA4
 *
 * 两个引脚由 MX_GPIO_Init() 配置为「开漏输出 + 内部上拉」，
 * 因此主机拉高即释放总线，可以直接读回电平做 ACK 检测，
 * 无需像推挽输出那样来回切换输入/输出方向。
 *
 * 注意：位翻转时序靠空循环延时保证，中间不能被长时间打断。
 *       这也是后续移植 RTOS 时需要单独处理的地方。
 */

void    SoftI2C_Start(void);
void    SoftI2C_Stop(void);
uint8_t SoftI2C_WaitAck(void);
void    SoftI2C_SendByte(uint8_t dat);

#endif

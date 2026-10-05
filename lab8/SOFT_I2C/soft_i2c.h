#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H

#include "main.h"

/*
 * 软件模拟 I²C（位翻转），本项目仅用于驱动 OLED
 *   SCL = PA3，SDA = PA4
 *
 * 两个引脚由 MX_GPIO_Init() 配置为开漏输出，
 * 因此主机拉高即释放总线，可以直接读回电平做 ACK 检测，
 * 无需像推挽输出那样来回切换输入/输出方向。
 *
 * ⚠ gpio.c 里那行 GPIO_PULLUP 在 STM32F1 上是空操作：F1 没有 PUPDR 寄存器，
 *   弱上拉只在【输入模式】可用，而且 HAL 的 GPIO_MODE_OUTPUT_OD 分支
 *   根本不读 GPIO_Init->Pull。总线的实际高电平靠原理图上外接的上拉电阻
 *   （手册里的 OLED 电路有这两个电阻），别以为配了 Pull 就有了。
 *
 * 关于 RTOS：位翻转中途被高优先级任务抢占是安全的。
 * 这里是【主机】被抢占，SDA/SCL 只是维持原电平更久、节拍变慢而已。
 * I²C 规范没有规定时钟低电平的最大时间（有总线超时要求的是 SMBus，
 * 不是 I²C），SSD1306 会一直等着。所以本模块不需要关中断保护。
 *
 * （别和"时钟拉伸"搞混：那是【从机】主动把 SCL 拉低、要求主机多等一会儿，
 *   方向和这里正好相反。）
 */

void    SoftI2C_Start(void);
void    SoftI2C_Stop(void);
uint8_t SoftI2C_WaitAck(void);
void    SoftI2C_SendByte(uint8_t dat);

#endif

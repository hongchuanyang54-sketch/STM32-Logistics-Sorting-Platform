#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H

#include "main.h"

/*
 * 软件模拟 I²C（位翻转），lab7 用来驱动 24C02C 掉电保存计数。
 *   SCL = PA4，SDA = PA5
 *
 * 为什么是软件模拟而不是硬件 I²C 外设：
 *   STM32F103 的 I2C1 只能用 PB6/PB7 或重映射后的 PB8/PB9，
 *   而这四个脚在 lab7 里全被数码管占了（PB6/PB7 = 段选 G/DP，
 *   PB8/PB9 = 位选 LCD1/LCD2）。PA4/PA5 本身不具备 I²C 复用功能，
 *   所以只能用 GPIO 位翻转来产生时序。
 *
 * 电气配置由 MX_GPIO_Init() 完成（PA4/PA5 = 开漏输出，初始为高），
 * 因此本模块不需要自己的 Init 函数。
 *   开漏  —— 主机只能拉低或释放，多设备不会互相短路
 *   可直接读回引脚电平，所以读数据 / 检测 ACK 都不用切换 GPIO 方向
 *
 * ⚠ 总线上【必须外接上拉电阻】：SCL 和 SDA 各接一个 4.7kΩ 到 3.3V。
 *
 *   .ioc 里 PA4/PA5 虽然填了 GPIO_PULLUP，但那是空操作，别指望它：
 *     1) STM32F1 没有 PUPDR 寄存器，弱上拉/下拉只在【输入模式】下可用；
 *     2) 就算有，HAL 的 GPIO_MODE_OUTPUT_OD 分支也根本不读 GPIO_Init->Pull
 *        （见 Drivers/.../stm32f1xx_hal_gpio.c 里那个 switch）。
 *   所以引脚被"释放"之后，没有任何东西把线拉回高电平，
 *   从机永远等不到上升沿 —— I²C 完全不通，且因为写失败被吞掉，
 *   现象很隐蔽：数码管、按键、串口一切正常，只是计数永远存不住。
 */

/* 半周期忙等循环次数。
   CPU 72MHz。按反汇编数：循环体每次约 11 个周期（-O0），50 次 ≈ 7.9μs；
   再加上前后各一次 HAL_GPIO_WritePin（约 50 周期），一个 bit 约 26μs，
   即 SCL 约 39kHz —— 离 24C02 的 100kHz 上限还有一倍多余量。
   24C02 没有最低频率要求，慢一点只是存得慢，不会出错。

   注意：这个值是靠忙等凑出来的，改优化等级（-O0 ↔ -O3）会改变实际频率。
   循环变量必须 volatile，否则整个循环会被优化掉，时序直接失控。 */
#define SOFT_I2C_DELAY_LOOPS  50

/* ACK 检测结果 */
#define SOFT_I2C_ACK    0   /* 从机把 SDA 拉低了，应答正常 */
#define SOFT_I2C_NACK   1   /* 没人应答：设备不在、地址不对、或还在忙 */

void    SoftI2C_Start(void);
void    SoftI2C_Stop(void);

/**
  * @brief  发送一个字节，并把第 9 个时钟从机的应答读回来
  * @param  dat 要发送的字节，MSB 先行
  * @retval SOFT_I2C_ACK / SOFT_I2C_NACK
  */
uint8_t SoftI2C_WriteByte(uint8_t dat);

/**
  * @brief  接收一个字节，并由主机回一个应答位
  * @param  ack 1 = 收到后回 ACK，告诉从机"继续发"
  *             0 = 收到后回 NACK，告诉从机"够了，这是最后一个字节"
  * @retval 收到的字节
  * @note   函数内部会先把 SDA 释放给从机驱动，返回前再释放一次，
  *         避免残留低电平影响紧接着的 STOP 条件。
  */
uint8_t SoftI2C_ReadByte(uint8_t ack);

#endif

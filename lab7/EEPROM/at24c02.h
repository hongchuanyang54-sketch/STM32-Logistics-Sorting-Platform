#ifndef __AT24C02_H
#define __AT24C02_H

#include "main.h"

/*
 * 24C02C I²C EEPROM 驱动（2Kbit = 256 字节）
 *
 * 硬件连接（见 lab7 原理图 U5）：
 *   A0/A1/A2 三脚并联接地 → 从机地址固定为 1010 000 = 0x50
 *   WP 接地               → 写使能（拉高则芯片锁死，只能读）
 *   SDA = PA5，SCL = PA4
 *
 * ⚠ 原理图上 SCL/SDA 目前【没有上拉电阻】，必须补上（各 4.7kΩ 到 3.3V）。
 *   STM32F1 的开漏输出配不出内部上拉，靠它拉高是不成立的，
 *   详见 soft_i2c.h 顶部的说明。
 *
 * 地址换算：24C02 的 8 位地址左移一位、低位补方向位，就是总线上实际发的字节。
 */

/* 写操作地址（方向位 0）和读操作地址（方向位 1） */
#define AT24C02_ADDR_W   0xA0
#define AT24C02_ADDR_R   0xA1

/* 计数值在 EEPROM 里存放的字节地址（256 字节里随便挑一个） */
#define AT24C02_COUNT_ADDR   0x00

/* 芯片写完一个字节后要等它内部烧写，这段时间它不响应任何命令 */
#define AT24C02_WRITE_CYCLE_MS   5

/**
  * @brief  往 EEPROM 指定地址写一个字节
  * @param  mem_addr 片内地址 0~255
  * @param  dat      要写入的字节
  * @retval 0 成功；-1 从机没应答（器件不在、WP 被拉高、或上一次写还没完成）
  * @note   函数返回时已经等完 tWR（写周期），可以直接发下一条命令。
  *         这个等待用的是 osDelay，会把 CPU 让给别的任务，
  *         因此本函数只能在调度器启动之后调用。
  */
int AT24C02_WriteByte(uint8_t mem_addr, uint8_t dat);

/**
  * @brief  从 EEPROM 指定地址读一个字节
  * @param  mem_addr 片内地址 0~255
  * @retval 0~255 读到的数据；-1 从机没应答
  * @note   返回 int 而不是 uint8_t，就是为了让 -1 能和合法的 0xFF 区分开。
  *         纯读操作不需要延时，调度器启动前也能安全调用。
  */
int AT24C02_ReadByte(uint8_t mem_addr);

/**
  * @brief  把计数值存进 EEPROM
  */
int AT24C02_SaveCount(uint8_t count);

/**
  * @brief  从 EEPROM 读回上次保存的计数值
  * @retval 0~255 读到的原始值；-1 读取失败
  * @note   空片（从没写过）读回来是全 1，即 0xFF = 255。
  *         本函数不做范围检查，请调用方自己判断合法性。
  */
int AT24C02_LoadCount(void);

#endif

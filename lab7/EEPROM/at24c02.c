/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    at24c02.c
  * @brief   24C02C I²C EEPROM 驱动（掉电保存计数值）
  ******************************************************************************
  */
/* USER CODE END Header */
#include "at24c02.h"
#include "soft_i2c.h"
#include "cmsis_os2.h"

/**
  * @brief  往 EEPROM 指定地址写一个字节
  * @note   时序：START → 0xA0 → 等ACK → 片内地址 → 等ACK → 数据 → 等ACK → STOP
  *         然后等 tWR。24C02 是"字节写"，一次只进一个字节，
  *         所以不存在跨页问题（它内部一页 8 字节）。
  *
  *         随便哪一步没等到 ACK 都说明器件没在听，此时要补一个 STOP
  *         把总线放回空闲态，否则残留的时序会让下一次访问也失败。
  */
int AT24C02_WriteByte(uint8_t mem_addr, uint8_t dat)
{
    SoftI2C_Start();

    if (SoftI2C_WriteByte(AT24C02_ADDR_W) != SOFT_I2C_ACK) goto fail;
    if (SoftI2C_WriteByte(mem_addr)       != SOFT_I2C_ACK) goto fail;
    if (SoftI2C_WriteByte(dat)            != SOFT_I2C_ACK) goto fail;

    SoftI2C_Stop();

    /* 等芯片把数据烧进存储单元。这期间它不响应任何命令，
       不等就发下一条必然 NACK。用 osDelay 让出 CPU，数码管照刷。 */
    osDelay(AT24C02_WRITE_CYCLE_MS);

    return 0;

fail:
    SoftI2C_Stop();
    return -1;
}

/**
  * @brief  从 EEPROM 指定地址读一个字节
  * @note   时序：START → 0xA0 → 等ACK → 片内地址 → 等ACK
  *                    → 重复START → 0xA1 → 等ACK → 读数据 → 主机回NACK → STOP
  *
  *         这里的"重复 START"（Restart，又叫 Sr）是 I²C 的标准动作：
  *         不发 STOP，直接在 SCL 高电平时再翻一次 SDA，重新开始一帧。
  *         为什么要这么绕？因为要先告诉芯片"我要读哪个地址"（那是写操作），
  *         再切换成读操作。如果中间插了 STOP，芯片就退出寻址状态了。
  */
int AT24C02_ReadByte(uint8_t mem_addr)
{
    uint8_t dat;

    SoftI2C_Start();

    if (SoftI2C_WriteByte(AT24C02_ADDR_W) != SOFT_I2C_ACK) goto fail;
    if (SoftI2C_WriteByte(mem_addr)       != SOFT_I2C_ACK) goto fail;

    /* 重复起始条件：不发 STOP，直接再来一次 START */
    SoftI2C_Start();

    if (SoftI2C_WriteByte(AT24C02_ADDR_R) != SOFT_I2C_ACK) goto fail;

    /* 只读一个字节：收完回 NACK，告诉从机"够了，你别再发了" */
    dat = SoftI2C_ReadByte(0);

    SoftI2C_Stop();

    return (int)dat;

fail:
    SoftI2C_Stop();
    return -1;
}

int AT24C02_SaveCount(uint8_t count)
{
    return AT24C02_WriteByte(AT24C02_COUNT_ADDR, count);
}

int AT24C02_LoadCount(void)
{
    return AT24C02_ReadByte(AT24C02_COUNT_ADDR);
}

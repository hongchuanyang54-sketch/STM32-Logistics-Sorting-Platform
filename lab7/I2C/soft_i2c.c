/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    soft_i2c.c
  * @brief   软件模拟 I²C（位翻转），用于驱动 24C02C EEPROM
  * @note    SCL = PA4，SDA = PA5，两脚的开漏 + 上拉配置在 MX_GPIO_Init() 里，
  *          本文件只负责产生时序。
  ******************************************************************************
  */
/* USER CODE END Header */
#include "soft_i2c.h"

/**
  * @brief  位翻转的间隔延时
  * @note   用忙等空循环撑出 I²C 的节拍。
  *         循环变量声明为 volatile，否则编译器优化后会把整个循环删掉，
  *         时序直接失控（-O0 能跑、-O3 就崩，这种 bug 最难查）。
  *
  *         软件 I²C 的固有弱点：时序靠 CPU 忙等保证。
  *         在本工程里这个任务会被数码管扫描任务抢占，
  *         后果只是 SCL 被拉长 —— I²C 允许时钟拉伸，从机不会出错，只是慢一点。
  */
void SoftI2C_Delay(void)
{
    volatile uint32_t i;
    for (i = 0; i < SOFT_I2C_DELAY_LOOPS; i++) { }
}

/**
  * @brief  起始条件：SCL 为高时，SDA 由高变低
  * @note   "SCL 高电平期间 SDA 变化"这个组合在正常传数据时不会出现
  *         （数据只在 SCL 低电平期间改），所以拿它当帧起始标志。
  */
void SoftI2C_Start(void)
{
    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_RESET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET);
    SoftI2C_Delay();
}

/**
  * @brief  停止条件：SCL 为高时，SDA 由低变高
  */
void SoftI2C_Stop(void)
{
    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET);
    SoftI2C_Delay();
}

/**
  * @brief  发送一个字节并读回应答位
  * @note   数据在 SCL 低电平期间改变，在 SCL 高电平期间保持稳定，
  *         从机趁高电平采样。发完 8 位后主机释放 SDA，
  *         第 9 个时钟由从机决定拉低（ACK）还是不拉（NACK）。
  */
uint8_t SoftI2C_WriteByte(uint8_t dat)
{
    uint8_t i;
    uint8_t ack;

    for (i = 0; i < 8; i++)
    {
        if (dat & 0x80)
        {
            HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET);
        }
        else
        {
            HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_RESET);
        }
        dat <<= 1;
        SoftI2C_Delay();

        HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET);
        SoftI2C_Delay();
        HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET);
        SoftI2C_Delay();
    }

    /* 第 9 个时钟：释放 SDA，让从机有机会把它拉低 */
    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET);
    SoftI2C_Delay();

    ack = (HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin) == GPIO_PIN_RESET)
              ? SOFT_I2C_ACK : SOFT_I2C_NACK;

    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET);
    SoftI2C_Delay();

    return ack;
}

/**
  * @brief  接收一个字节并回一个应答位
  * @note   读方向上，SDA 由从机驱动：主机只管发时钟，
  *         在 SCL 高电平期间把引脚电平读进来。
  */
uint8_t SoftI2C_ReadByte(uint8_t ack)
{
    uint8_t i;
    uint8_t dat = 0;

    /* 主机先释放 SDA，接下来 8 个时钟由从机驱动它 */
    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET);

    for (i = 0; i < 8; i++)
    {
        SoftI2C_Delay();
        HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET);
        SoftI2C_Delay();

        dat <<= 1;
        if (HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin) == GPIO_PIN_SET)
        {
            dat |= 0x01;
        }

        HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET);
    }
    SoftI2C_Delay();

    /* 第 9 个时钟：ACK 是主机把 SDA 拉低，NACK 是不动作（靠上拉回高） */
    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin,
                      ack ? GPIO_PIN_RESET : GPIO_PIN_SET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET);
    SoftI2C_Delay();

    /* 收尾释放 SDA，否则紧接着的 STOP 会变成"SCL 高时 SDA 已经是低的"，
       从机可能识别不出停止条件 */
    HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET);

    return dat;
}

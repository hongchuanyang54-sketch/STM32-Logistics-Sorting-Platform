#include "soft_i2c.h"

/* PA3 = SCL、PA4 = SDA（User Label 定义在 CubeMX 生成的 main.h 中） */
#define SCL_H()     HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET)
#define SCL_L()     HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET)
#define SDA_H()     HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET)
#define SDA_L()     HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_RESET)
#define SDA_READ()  HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin)

/**
  * @brief  位翻转的间隔延时
  * @note   全靠这个空循环撑出时序，约几百 kHz。
  *         SSD1306 没有最小时钟频率要求，慢一点不影响显示。
  */
static void I2C_Delay(void)
{
    volatile uint32_t i = 100;
    while (i--) { __NOP(); }
}

/**
  * @brief  起始条件：SCL 为高时，SDA 由高变低
  */
void SoftI2C_Start(void)
{
    SDA_H();
    SCL_H();
    I2C_Delay();
    SDA_L();
    I2C_Delay();
    SCL_L();
}

/**
  * @brief  停止条件：SCL 为高时，SDA 由低变高
  */
void SoftI2C_Stop(void)
{
    SDA_L();
    SCL_H();
    I2C_Delay();
    SDA_H();
    I2C_Delay();
}

/**
  * @brief  第 9 个时钟：释放 SDA 后读取从机应答
  * @retval 0 = 收到 ACK，非 0 = NACK
  */
uint8_t SoftI2C_WaitAck(void)
{
    uint8_t ack;

    SDA_H();                 /* 主机释放 SDA，交给从机拉低 */
    I2C_Delay();
    SCL_H();
    I2C_Delay();
    ack = SDA_READ();
    SCL_L();
    I2C_Delay();

    return ack;
}

/**
  * @brief  发送一个字节，MSB 先行
  * @note   发送完 8 位后释放 SDA 并产生第 9 个时钟，交由调用方
  *         （SoftI2C_WaitAck）读取应答
  */
void SoftI2C_SendByte(uint8_t dat)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (dat & 0x80) SDA_H();
        else            SDA_L();

        dat <<= 1;
        I2C_Delay();
        SCL_H();
        I2C_Delay();
        SCL_L();
        I2C_Delay();
    }
}

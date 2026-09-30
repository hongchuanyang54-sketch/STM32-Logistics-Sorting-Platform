/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    soft_i2c.c
  * @brief   软件模拟 I²C（位翻转），用于在 Proteus 中驱动 OLED
  * @note    STM32F103 的硬件 I²C 在本实验环境下不便仿真，
  *          因此用 GPIO 模拟时序，SCL = PA6、SDA = PA7。
  ******************************************************************************
  */
/* USER CODE END Header */
#include "soft_i2c.h"

/**
  * @brief  位翻转的间隔延时
  * @note   全靠这个空循环撑出时序。SSD1306 没有最小时钟频率要求，
  *         慢一点不影响显示。软件 I²C 的固有弱点也在这里：
  *         时序由 CPU 忙等保证，中间被长时间打断就会破坏总线。
  */
void SoftI2C_Delay(void)
{
    uint32_t i;
    for (i = 0; i < 5; i++) { }
}

/**
  * @brief  初始化 I²C 引脚并释放总线
  * @note   PA6/PA7 配成开漏 + 上拉：拉高即释放总线，可直接读回电平。
  */
void SoftI2C_Init(void)
{
    GPIO_InitTypeDef gpio_cfg = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio_cfg.Pin   = SCL_PIN | SDA_PIN;
    gpio_cfg.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio_cfg.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_cfg.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(I2C_PORT, &gpio_cfg);

    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN | SDA_PIN, GPIO_PIN_SET);
}

/**
  * @brief  起始条件：SCL 为高时，SDA 由高变低
  */
void SoftI2C_Start(void)
{
    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_SET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_RESET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_RESET);
}

/**
  * @brief  停止条件：SCL 为高时，SDA 由低变高
  */
void SoftI2C_Stop(void)
{
    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_SET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_SET);
    SoftI2C_Delay();
}

/**
  * @brief  发送一个字节，MSB 先行
  * @note   8 位数据发完后释放 SDA 并补一个时钟，作为从机应答位。
  *         本驱动不检查从机是否应答（SSD1306 不会 NACK 写操作）。
  */
void SoftI2C_WriteByte(uint8_t dat)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        if (dat & 0x80)
        {
            HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_SET);
        }
        else
        {
            HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_RESET);
        }
        dat <<= 1;
        SoftI2C_Delay();
        HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_SET);
        SoftI2C_Delay();
        HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_RESET);
    }

    /* 第 9 个时钟：主机释放 SDA，留给从机拉低应答 */
    SoftI2C_Delay();
    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, GPIO_PIN_SET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_SET);
    SoftI2C_Delay();
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, GPIO_PIN_RESET);
}

/**
  * @brief  在一次事务里连续写出多个字节
  * @note   见 soft_i2c.h 的说明：调用方负责 START/地址/控制字节和 STOP。
  */
void SoftI2C_WriteBulk(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++)
    {
        SoftI2C_WriteByte(data[i]);
    }
}

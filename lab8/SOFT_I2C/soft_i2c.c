#include "soft_i2c.h"

/* PA3 = SCL、PA4 = SDA（User Label 定义在 CubeMX 生成的 main.h 中） */
#define SCL_H() HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_SET)
#define SCL_L() HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, GPIO_PIN_RESET)
#define SDA_H() HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET)
#define SDA_L() HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_RESET)
#define SDA_READ() HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin)

/* 半周期忙等循环次数。按 -O0 / 72MHz 反汇编数周期算：

     循环体 6 条指令 ≈ 10 周期
     一次 I2C_Delay   = N×10 + 开销(~23) 周期
     一个 bit = 3×(I2C_Delay + HAL_GPIO_WritePin约45周期)

   原来 N=100：一次延时 ~1023 周期(14µs)，一个 bit ~44µs → SCL 仅 22kHz，
              刷一屏（43 字符 × 9 次事务 × 31 bit）要 ~0.5 秒。
              这是本工程最大的性能瓶颈，也是裸机版"电机 15ms 一拍"
              实际被拖成 ~500ms 一拍的根因。
   现在 N=20： 一次延时 ~223 周期(3.1µs)，一个 bit ~11.2µs → SCL 约 89kHz，
              刷一屏降到 ~0.13 秒（4 倍提速）。

   89kHz 离 SSD1306 的 400kHz 上限还有很大余量；再往下调收益递减
   （GPIO 调用的开销已经开始占主导），所以停在这里。

   注意：这个值是靠忙等凑出来的，改优化等级（-O0 ↔ -O3）会改变实际频率，
   所以循环变量必须 volatile，否则整个循环会被优化掉、时序直接失控。 */
#define I2C_DELAY_LOOPS 20

/**
 * @brief  位翻转的间隔延时
 */
static void I2C_Delay(void) {
  volatile uint32_t i = I2C_DELAY_LOOPS;
  while (i--) {
    __NOP();
  }
}

/**
 * @brief  起始条件：SCL 为高时，SDA 由高变低
 */
void SoftI2C_Start(void) {
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
void SoftI2C_Stop(void) {
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
uint8_t SoftI2C_WaitAck(void) {
  uint8_t ack;

  SDA_H(); /* 主机释放 SDA，交给从机拉低 */
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
void SoftI2C_SendByte(uint8_t dat) {
  for (uint8_t i = 0; i < 8; i++) {
    if (dat & 0x80)
      SDA_H(); // 这一步是判断 dat 的最高位是否为 1，如果是，则将 SDA
               // 拉高；否则，将 SDA 拉低。
    else
      SDA_L();

    dat <<= 1;
    I2C_Delay();
    SCL_H();
    I2C_Delay();
    SCL_L();
    I2C_Delay();
  }
}

# 传送带控制器固件
# HAL 源文件清单与 MDK-ARM/summer project9.uvprojx 保持一致

stm32_add_firmware(
    TARGET  lab8
    LAB     ${CMAKE_CURRENT_SOURCE_DIR}

    HAL_SRC
        stm32f1xx_hal.c
        stm32f1xx_hal_adc.c
        stm32f1xx_hal_adc_ex.c
        stm32f1xx_hal_cortex.c
        stm32f1xx_hal_dma.c
        stm32f1xx_hal_exti.c
        stm32f1xx_hal_flash.c
        stm32f1xx_hal_flash_ex.c
        stm32f1xx_hal_gpio.c
        stm32f1xx_hal_gpio_ex.c
        stm32f1xx_hal_pwr.c
        stm32f1xx_hal_rcc.c
        stm32f1xx_hal_rcc_ex.c
        stm32f1xx_hal_tim.c
        stm32f1xx_hal_tim_ex.c
        stm32f1xx_hal_uart.c

    APP_SRC
        Core/Src/main.c
        Core/Src/gpio.c
        Core/Src/adc.c
        Core/Src/tim.c
        Core/Src/usart.c
        Core/Src/stm32f1xx_it.c
        Core/Src/stm32f1xx_hal_msp.c
        MOTOR/motor.c
        OLED/oled.c
        SOFT_I2C/soft_i2c.c

    APP_INC
        MOTOR
        OLED
        SOFT_I2C
)

# 光电计数器固件
# HAL 源文件清单与 MDK-ARM/T7.uvprojx 保持一致

stm32_add_firmware(
    TARGET  lab7
    LAB     ${CMAKE_CURRENT_SOURCE_DIR}

    HAL_SRC
        stm32f1xx_hal.c
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
        stm32f1xx_hal_uart.c

    APP_SRC
        Core/Src/main.c
        Core/Src/gpio.c
        Core/Src/usart.c
        Core/Src/stm32f1xx_it.c
        Core/Src/stm32f1xx_hal_msp.c
        KEY/key.c
        DISPLAY/display.c
        UART_SEND/uart_send.c

    APP_INC
        KEY
        DISPLAY
        UART_SEND
)

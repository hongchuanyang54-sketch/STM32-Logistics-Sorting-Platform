# =============================================================================
# 光电计数器固件（lab7）—— 顶层 CMakeLists
#
# 分工：
#   cmake/stm32cubemx/CMakeLists.txt  ← CubeMX 生成并维护：
#          HAL 驱动、FreeRTOS、Core/、启动文件、include 路径、宏定义、链接选项
#   本文件                            ← 只放 CubeMX 不知道的东西：
#          手写模块（KEY / DISPLAY / UART_SEND）和 hex/bin 产出
#
# 所以每次在 CubeMX 里重新生成代码，只需要管这一个文件以外的东西它都自己更新。
#
# 用法：
#   cmake --preset Debug && cmake --build --preset Debug
# 产物：lab7.elf / lab7.hex / lab7.bin / lab7.map
# =============================================================================
cmake_minimum_required(VERSION 3.22)

# 工程名必须和可执行目标名一致：
# CubeMX 生成的子工程是用 target_sources(${CMAKE_PROJECT_NAME} ...) 挂源文件的
project(lab7 C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 空目标，源文件由 CubeMX 的子工程和下面几行填进来
add_executable(${CMAKE_PROJECT_NAME})

# ---- CubeMX 维护的部分 ----
add_subdirectory(cmake/stm32cubemx)

# ---- 手写模块（CubeMX 不管这些，每次加新模块记得在这里挂上）----
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    KEY/key.c
    DISPLAY/display.c
    UART_SEND/uart_send.c
    I2C/soft_i2c.c
    EEPROM/at24c02.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    KEY
    DISPLAY
    UART_SEND
    I2C
    EEPROM
)

# ---- 产出 hex / bin，Proteus 直接加载 ----
set(FIRMWARE_HEX ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex)
set(FIRMWARE_BIN ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin)
add_custom_command(
    OUTPUT ${FIRMWARE_HEX} ${FIRMWARE_BIN}
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${FIRMWARE_HEX}
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${FIRMWARE_BIN}
    DEPENDS ${CMAKE_PROJECT_NAME}
    COMMENT "生成 ${CMAKE_PROJECT_NAME}.hex / .bin"
    VERBATIM
)
add_custom_target(${CMAKE_PROJECT_NAME}_firmware_images ALL
    DEPENDS ${FIRMWARE_HEX} ${FIRMWARE_BIN}
)
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    VERBATIM
)

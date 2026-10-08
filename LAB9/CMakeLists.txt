# =============================================================================
# 分拣器控制器固件（LAB9）—— 独立 CMake 工程
#
# 分工：
#   cmake/stm32cubemx/  ← CubeMX 生成并维护：HAL、FreeRTOS、Core/、启动文件、
#                          include 路径、宏定义、链接脚本
#   本文件              ← 只放 CubeMX 不知道的东西：手写模块 + hex/bin 产出
#
# CubeMX 里重新生成代码不会覆盖本文件（它只在首次转换时写一次），
# 但每次新加手写模块，记得在下面 target_sources / target_include_directories 里补一行。
#
# 用法：
#   cmake --preset Debug && cmake --build --preset Debug
# 产物：LAB9.elf / LAB9.hex / LAB9.bin
# =============================================================================
cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# 工程名必须和可执行目标名一致（CubeMX 的子工程用 target_sources 往这里挂源文件）
set(CMAKE_PROJECT_NAME LAB9)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

enable_language(C ASM)

add_executable(${CMAKE_PROJECT_NAME})

# ---- CubeMX 维护的部分 ----
add_subdirectory(cmake/stm32cubemx)

# ---- 手写模块 ----
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    OLED/oled.c
    SOFT_I2C/soft_i2c.c
    JSON_PARSER/json_parser.c
    UART_RX/uart_rx.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    OLED
    SOFT_I2C
    JSON_PARSER
    UART_RX
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
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

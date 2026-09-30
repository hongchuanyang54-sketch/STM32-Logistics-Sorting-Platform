# =============================================================================
# 物流分拣平台 —— 三个独立固件目标
#
#   lab7  光电计数器
#   lab8  传送带控制器
#   LAB9  分拣器控制器
#
# 配置（VSCode 里直接选 preset 即可）：
#   cmake --preset debug
#   cmake --build --preset debug
#   cmake --build --preset debug --target lab8     # 只编某一个
#
# 产物在 build/<preset>/ 下：<target>.elf / .hex / .bin / .map
# 其中 .hex 可以直接加载进 Proteus 的 STM32 元件。
# =============================================================================
cmake_minimum_required(VERSION 3.20)

project(STM32_Logistics_Sorting_Platform
        DESCRIPTION "物流分拣平台：光电计数器 / 传送带控制器 / 分拣器控制器"
        LANGUAGES C ASM)

# 没走工具链文件说明用的是主机编译器，裸机工程必然编不过，直接拦下
if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "这个工程必须交叉编译。\n"
        "  VSCode：选择 CMakePresets.json 里的 preset（如 debug）\n"
        "  命令行：cmake --preset debug\n"
        "  或手动指定：-DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 生成 compile_commands.json，供 VSCode / clangd 做代码跳转与补全
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# 共用函数 stm32_add_firmware() 在 cmake/ 下
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(stm32_add_firmware)

# lab7 已切换到 CubeMX 自己维护的 CMake 工程（lab7/CMakeLists.txt +
# lab7/cmake/stm32cubemx/），因为它在 CubeMX 里开启了 FreeRTOS，
# 源文件清单由 CubeMX 自动更新，不再手工维护。
# 它现在是独立工程，用法：cd lab7 && cmake --preset Debug && cmake --build --preset Debug
#
# 下面两个 lab 暂时还走这套统一构建，等它们也开启 FreeRTOS 时一并迁移。
add_subdirectory(lab8)
add_subdirectory(LAB9)

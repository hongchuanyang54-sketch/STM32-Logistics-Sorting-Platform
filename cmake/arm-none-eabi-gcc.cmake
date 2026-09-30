# =============================================================================
# CMake 工具链文件：arm-none-eabi-gcc（Cortex-M3）
#
# 用法：
#   cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
# 或在 VSCode 里用 CMakePresets.json（已配好，推荐）
# =============================================================================

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 编译器探测阶段不要让 CMake 尝试链接出可执行文件
# （裸机目标没有默认链接脚本，链接测试必然失败）
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# ---- 定位工具链 ----
# 优先用 PATH 上的 arm-none-eabi-gcc；找不到再回退到本机已知的安装位置。
# 需要覆盖时：cmake -DTOOLCHAIN_PATH=/your/path/bin ...
if(NOT TOOLCHAIN_PATH)
    find_program(_ARM_GCC arm-none-eabi-gcc)
    if(_ARM_GCC)
        get_filename_component(TOOLCHAIN_PATH "${_ARM_GCC}" DIRECTORY)
    else()
        set(TOOLCHAIN_PATH "D:/DevEnv/DevEnv/GNU-tools-for-STM32/bin")
    endif()
    unset(_ARM_GCC CACHE)
endif()

set(CMAKE_C_COMPILER   "${TOOLCHAIN_PATH}/arm-none-eabi-gcc")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PATH}/arm-none-eabi-gcc")
set(CMAKE_OBJCOPY      "${TOOLCHAIN_PATH}/arm-none-eabi-objcopy" CACHE FILEPATH "")
set(CMAKE_SIZE         "${TOOLCHAIN_PATH}/arm-none-eabi-size"    CACHE FILEPATH "")

if(NOT EXISTS "${CMAKE_C_COMPILER}.exe" AND NOT EXISTS "${CMAKE_C_COMPILER}")
    message(FATAL_ERROR
        "找不到 arm-none-eabi-gcc。\n"
        "  已尝试路径: ${TOOLCHAIN_PATH}\n"
        "  请把工具链加入 PATH，或用 -DTOOLCHAIN_PATH=<bin 目录> 指定。")
endif()

# 由 CMake 负责把 GCC 的 .exe 后缀补上
foreach(_tool CMAKE_C_COMPILER CMAKE_ASM_COMPILER CMAKE_OBJCOPY CMAKE_SIZE)
    if(NOT EXISTS "${${_tool}}" AND EXISTS "${${_tool}}.exe")
        set(${_tool} "${${_tool}}.exe")
    endif()
endforeach()

message(STATUS "工具链: ${CMAKE_C_COMPILER}")

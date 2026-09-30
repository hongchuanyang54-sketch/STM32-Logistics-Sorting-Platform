# =============================================================================
# stm32_add_firmware() —— 按 Keil 工程的原样创建一个 STM32F1 固件目标
#
# 源文件清单与各 lab 的 .uvprojx 保持一一对应，不靠通配符扫描，
# 这样「编译了哪些文件」在 CMake 和 Keil 下是同一份事实。
#
# 用法（在各 lab 自己的 CMakeLists.txt 里）：
#
#   stm32_add_firmware(
#     TARGET  lab7
#     LAB     ${CMAKE_CURRENT_SOURCE_DIR}
#     HAL_SRC stm32f1xx_hal.c stm32f1xx_hal_gpio.c ...   # 相对 HAL 的 Src 目录
#     APP_SRC Core/Src/main.c KEY/key.c ...              # 相对 lab 目录
#     APP_INC KEY DISPLAY UART_SEND                      # 相对 lab 目录
#   )
#
# 产物：<TARGET>.elf / .hex / .bin / .map，hex 可直接加载进 Proteus
# =============================================================================

include_guard(GLOBAL)

function(stm32_add_firmware)
    cmake_parse_arguments(ARG "" "TARGET;LAB" "HAL_SRC;APP_SRC;APP_INC" ${ARGN})

    if(NOT ARG_TARGET OR NOT ARG_LAB)
        message(FATAL_ERROR "stm32_add_firmware 需要 TARGET 和 LAB 参数")
    endif()

    set(_hal "${ARG_LAB}/Drivers/STM32F1xx_HAL_Driver/Src")
    set(_cmsis "${ARG_LAB}/Drivers/CMSIS")

    # ---- 源文件 ----
    set(_sources
        "${ARG_LAB}/Core/Src/system_stm32f1xx.c"
        "${_cmsis}/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xb.s"
    )
    foreach(_s IN LISTS ARG_HAL_SRC)
        list(APPEND _sources "${_hal}/${_s}")
    endforeach()
    foreach(_s IN LISTS ARG_APP_SRC)
        list(APPEND _sources "${ARG_LAB}/${_s}")
    endforeach()

    add_executable(${ARG_TARGET} ${_sources})

    # ---- 头文件路径 ----
    target_include_directories(${ARG_TARGET} PRIVATE
        "${ARG_LAB}/Core/Inc"
        "${_hal}/../Inc"
        "${_hal}/../Inc/Legacy"
        "${_cmsis}/Device/ST/STM32F1xx/Include"
        "${_cmsis}/Include"
    )
    foreach(_i IN LISTS ARG_APP_INC)
        target_include_directories(${ARG_TARGET} PRIVATE "${ARG_LAB}/${_i}")
    endforeach()

    # ---- 宏定义（与 Keil 工程一致）----
    target_compile_definitions(${ARG_TARGET} PRIVATE
        USE_HAL_DRIVER
        STM32F103xB
    )

    # ---- 编译选项 ----
    target_compile_options(${ARG_TARGET} PRIVATE
        -mcpu=cortex-m3
        -mthumb
        -mfloat-abi=soft
        -std=gnu11
        -Wall
        -fdata-sections
        -ffunction-sections
        -g3
        -gdwarf-2
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Release>:-Os>
    )

    # ---- 链接选项 ----
    # 用 newlib-nano：完整 newlib 会让 lab7 的 Flash 从 14KB 涨到 48KB（73%），
    # 而三个 lab 都要用 sscanf 的扫描集（"{\"Speed\":\"%[^\"]\"}"）。
    # nano 是否支持扫描集这点已经核实过：链接产物里出现了 newlib 的 sccl.c
    # （scan character class），这个文件只在程序真的用到 %[ 时才会被拉进来，
    # 反汇编 _scanf_chars 也能看到 256 字节字符类查表的实现。
    # 若将来换库或换工具链，重新确认一次这个符号即可。
    target_link_options(${ARG_TARGET} PRIVATE
        -mcpu=cortex-m3
        -mthumb
        -mfloat-abi=soft
        -T${CMAKE_SOURCE_DIR}/cmake/STM32F103C8Tx_FLASH.ld
        -Wl,-Map=${ARG_TARGET}.map,--cref
        -Wl,--gc-sections
        -Wl,--print-memory-usage
        -specs=nano.specs
        -specs=nosys.specs
    )

    # ---- 产出 hex/bin，并打印体积 ----
    add_custom_command(TARGET ${ARG_TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${ARG_TARGET}> ${ARG_TARGET}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${ARG_TARGET}> ${ARG_TARGET}.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${ARG_TARGET}>
        COMMENT "生成 ${ARG_TARGET}.hex / ${ARG_TARGET}.bin"
        VERBATIM
    )

    # 让 VSCode / clangd 能直接找到编译数据库
    set_target_properties(${ARG_TARGET} PROPERTIES
        OUTPUT_NAME ${ARG_TARGET}
        SUFFIX ".elf"
    )
endfunction()

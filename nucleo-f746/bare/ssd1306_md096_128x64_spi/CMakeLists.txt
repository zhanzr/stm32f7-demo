# SSD1306 0.96" 128x64 mono OLED (MD096) over SPI1 - NUCLEO-F746ZG @ 216 MHz.
# Integrates the nucleo144-f746zg repo's ssd1306_oled096_spi branch (hardware
# connection + SSD1306 command flow); display patterns follow the f425-start
# repo's bare/ssd1306_md096_128x64_iic demo.
#
# Workflow:
#   bash build.sh          # default: GNU arm-none-eabi-gcc
#   ninja flash            # programs the board (probe-rs; pick the nucleo
#                          #   probe with -DDEBUG_PROBE=<selector>)

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

set(STM32_TOOLCHAIN "gcc" CACHE STRING "C compiler: gcc (arm-none-eabi, default) or armclang (Keil AC6)")
set_property(CACHE STM32_TOOLCHAIN PROPERTY STRINGS gcc armclang)
option(STM32_LTO "Enable -flto with the GCC toolchain" OFF)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
endif()

project(ssd1306_md096_128x64_spi C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/oled.c
    src/oledfont.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32f746_board.cmake)
stm32f746_apply_board(${PROJECT_NAME}.elf "-O2")

# SPI HAL - only this project uses it.
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${F7_HAL}/Src/stm32f7xx_hal_spi.c
)

# Generic objcopy targets + this board's flash target.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)

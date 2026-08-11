# blink_hello: minimal STM32F769NI (STM32F769I-Discovery) template @ 216 MHz.
# Toggles the three on-board LEDs (LD1 PJ13, LD2 PJ5, LD3 PA12, all high
# active) and prints a line over USART1 (ST-Link VCOM @ 115200) on every
# toggle. Intended as the starting point for new disco projects: clock,
# MPU, caches, UART console, SysTick and the newlib stubs all come from the
# shared board layer (../board + ../cmake).
#
# Pico-style workflow:
#   bash build.sh                           # default: GNU arm-none-eabi-gcc
#   mkdir -p build-ac6 && cd build-ac6      # optional: Keil AC6 (armclang)
#   cmake -G Ninja -DSTM32_TOOLCHAIN=armclang ..
#   ninja           # builds .elf + .hex (hex is what `ninja flash` uses)
#   ninja flash     # programs the board (probe-rs, ST-Link V2 / SWD)
#   ninja bin       # optional raw .bin image, "in case" you need it

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

# Toolchain selection. gcc (default) = GNU arm-none-eabi end-to-end;
# armclang = Keil Arm Compiler 6 C + GNU as/ld. CMAKE_TOOLCHAIN_FILE is
# cached, so use a separate build dir per toolchain (build/ for gcc,
# build-ac6/ for armclang).
set(STM32_TOOLCHAIN "gcc" CACHE STRING "C compiler: gcc (arm-none-eabi, default) or armclang (Keil AC6)")
set_property(CACHE STM32_TOOLCHAIN PROPERTY STRINGS gcc armclang)
option(STM32_LTO "Enable -flto with the GCC toolchain" OFF)

if(NOT CMAKE_TOOLCHAIN_FILE)
    if(STM32_TOOLCHAIN STREQUAL "armclang")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-keil-toolchain.cmake")
    else()
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
    endif()
endif()

project(blink_hello C ASM)

if(STM32_TOOLCHAIN STREQUAL "armclang")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-postproject.cmake)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    src/main.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32f769_board.cmake)
stm32f769_apply_board(${PROJECT_NAME}.elf "-O2")

# ADC (internal channels: die temp / VREFINT / VBAT) - only this project uses it.
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_adc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_adc_ex.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)

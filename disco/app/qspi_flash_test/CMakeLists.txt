# qspi_flash_test: MX25L51245G (64 MB) QSPI NOR flash benchmark on the
# STM32F769I-Discovery. Measures erase / program / read throughput via the
# vendor BSP (stm32f769i_discovery_qspi) in 4-wire QPI mode. No memory-mapped
# (XIP) remap - indirect (FIFO) transfers only.
#
# Pico-style workflow:
#   bash build.sh       # default: GNU arm-none-eabi-gcc
#   ninja               # builds .elf + .hex
#   ninja flash         # programs the board (probe-rs, ST-Link V2 / SWD)

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

project(qspi_flash_test C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(VENDOR ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor)
set(QSPI_BSP ${VENDOR}/Drivers/BSP/STM32F769I-Discovery)
set(MX25L512 ${VENDOR}/Drivers/BSP/Components/mx25l512)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    ${QSPI_BSP}/stm32f769i_discovery_qspi.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32f769_board.cmake)
stm32f769_apply_board(${PROJECT_NAME}.elf "-O2")

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${QSPI_BSP}
    ${MX25L512}
)

# QSPI HAL (vendor BSP uses it). Only this project needs it.
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_qspi.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)

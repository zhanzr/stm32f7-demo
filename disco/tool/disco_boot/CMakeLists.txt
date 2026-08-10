# disco_boot: minimal STM32F769NI (STM32F769I-Discovery) bootloader in internal
# flash.
#
# Prints a banner, checks the MX25L51245G (QUADSPI) for a bootable stage-2
# firmware at the QSPI memory-mapped base 0x90000000, and jumps to it. If the
# check fails it loops, toggling the three LEDs and printing the check result.
#
# The stage-2 firmware is written to the MX25L51245G separately (probe-rs
# QUADSPI flash algorithm) - see ../qspi_map/algo/README.md.
#
# Targets:
#   ninja        - builds disco_boot.elf + disco_boot.hex (internal flash)
#   ninja flash  - probe-rs -> internal flash (STM32F769NI)

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

project(disco_boot C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32f769_board.cmake)

# The bootloader lives in (and owns) internal flash + the 216 MHz clock tree,
# so the default board linker script (stm32f769ni.ld) and the stock system
# init (system_stm32f7xx.c) are exactly what apply_board() already uses.

set(VENDOR ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor)
set(QSPI_BSP ${VENDOR}/Drivers/BSP/STM32F769I-Discovery)
set(MX25L512 ${VENDOR}/Drivers/BSP/Components/mx25l512)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    ${QSPI_BSP}/stm32f769i_discovery_qspi.c
)

stm32f769_apply_board(${PROJECT_NAME}.elf "-O2")

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${QSPI_BSP}
    ${MX25L512}
)

# QUADSPI HAL (vendor BSP uses it). Only the bootloader needs it.
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_qspi.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)

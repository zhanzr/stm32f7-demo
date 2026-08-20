# blink_hello: minimal STM32F722ZE (NUCLEO-F722ZE) template @ 216 MHz.
# Toggles the three on-board LEDs (LD1 PB0, LD2 PB7, LD3 PB14, high active)
# and prints a line over USART3 (PD8/PD9, ST-Link VCP @ 115200) on every
# toggle, plus the current core frequency and the on-chip ADC internal
# channels (die temperature / VREFINT / VBAT).
#
# The F7 HAL + CMSIS come from the shared STM32Cube_FW_F7 package (see
# ../../../cmake/stm32cubef7.cmake); the board layer (../../board) and this
# project use the nucleo-f722 own folder.
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
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../../disco-f769/cmake/arm-none-eabi-toolchain.cmake")
endif()

project(blink_hello C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    src/main.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32f722_board.cmake)
stm32f722_apply_board(${PROJECT_NAME}.elf "-O2")

# ADC (internal channels: die temp / VREFINT / VBAT) - only this project uses it.
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${F7_HAL}/Src/stm32f7xx_hal_adc.c
    ${F7_HAL}/Src/stm32f7xx_hal_adc_ex.c
)

# Generic objcopy targets (from the disco cmake) + this board's flash target.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../disco-f769/cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
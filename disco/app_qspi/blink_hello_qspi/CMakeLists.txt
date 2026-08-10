# blink_hello_qspi: minimal STM32F769NI (STM32F769I-Discovery) QSPI app
# @ 216 MHz. Same as app/blink_hello (LD1/2/3 + USART1 console) but linked for
# and booted from the MX25L51245G at 0x90000000 (QUADSPI memory-mapped), by
# disco_boot in internal flash.
#
# Pico-style workflow:
#   bash build.sh                           # default: GNU arm-none-eabi-gcc
#   ninja           # builds .elf + .hex (hex is what `ninja flash` uses)
#   ninja flash     # writes the MX25L51245G via the QUADSPI flash algorithm
#   ninja bin       # optional raw .bin image

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

project(blink_hello C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32f769_board.cmake)

# QSPI app: link at 0x90000000 and use the non-destructive SystemInit (the
# bootloader owns the 216 MHz clock tree / QUADSPI clock).
set(F769_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/app.ld" CACHE FILEPATH
    "QSPI app linker script (code at 0x90000000)" FORCE)
set(F769_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/system_app.c" CACHE FILEPATH
    "QSPI app system init (non-destructive)" FORCE)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/system_app.c
)

stm32f769_apply_board(${PROJECT_NAME}.elf "-O2")

# QSPI app: make board.c's SystemClock_Config()/MPU a no-op so the identical
# main() can call Board_Init() without breaking the bootloader's clocks/MPU.
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE QSPI_APP=1)

# ADC (internal channels: die temp / VREFINT / VBAT) - same as app/blink_hello.
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_adc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_adc_ex.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/qspi-flash-targets.cmake)

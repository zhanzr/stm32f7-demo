# dhry_216m_qspi: Dhrystone 2.1 benchmark for the STM32F769NI (STM32F769I-
# Discovery) @ 216 MHz, linked for and booted from the MX25L51245G at
# 0x90000000 (QUADSPI memory-mapped), by disco_boot in internal flash.
#
# Pico-style workflow:
#   bash build.sh       # default: GNU arm-none-eabi-gcc
#   ninja               # builds .elf + .hex
#   ninja flash         # writes the MX25L51245G via the QUADSPI algorithm

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

project(dhry_216m C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32f769_board.cmake)

# QSPI app: link at 0x90000000 and use the non-destructive SystemInit.
set(F769_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/app.ld" CACHE FILEPATH
    "QSPI app linker script (code at 0x90000000)" FORCE)
set(F769_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/system_app.c" CACHE FILEPATH
    "QSPI app system init (non-destructive)" FORCE)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/utils.c
    src/dhry_1.c
    src/dhry_2.c
    src/system_app.c
)

stm32f769_apply_board(${PROJECT_NAME}.elf "-Ofast -ffp-contract=fast -funroll-loops")

# QSPI app: make board.c's SystemClock_Config()/MPU a no-op.
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE QSPI_APP=1)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/qspi-flash-targets.cmake)

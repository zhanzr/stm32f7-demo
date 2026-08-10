# lcd_touch_test_qspi: MIPI DSI LCD dynamic demo + FT6206 touch for the
# STM32F769NI (STM32F769I-Discovery) @ 216 MHz, linked for and booted from the
# MX25L51245G at 0x90000000 (QUADSPI memory-mapped), by disco_boot. Same demo
# as app/lcd_touch_test - the app code runs from QSPI while the LTDC/DSI use
# the SDRAM framebuffer.
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

project(lcd_touch_test C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32f769_board.cmake)

# QSPI app: link at 0x90000000 and use the non-destructive SystemInit.
set(F769_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/app.ld" CACHE FILEPATH
    "QSPI app linker script (code at 0x90000000)" FORCE)
set(F769_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/system_app.c" CACHE FILEPATH
    "QSPI app system init (non-destructive)" FORCE)

set(VENDOR ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor)
set(BSP_DIR ${VENDOR}/Drivers/BSP/STM32F769I-Discovery)
set(COMP    ${VENDOR}/Drivers/BSP/Components)
set(FONTS   ${VENDOR}/Utilities/Fonts)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/system_app.c
    ${BSP_DIR}/stm32f769i_discovery.c
    ${BSP_DIR}/stm32f769i_discovery_lcd.c
    ${BSP_DIR}/stm32f769i_discovery_sdram.c
    ${BSP_DIR}/stm32f769i_discovery_ts.c
    ${COMP}/otm8009a/otm8009a.c
    ${COMP}/nt35510/nt35510.c
    ${COMP}/ft6x06/ft6x06.c
)

stm32f769_apply_board(${PROJECT_NAME}.elf "-O2")

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${BSP_DIR}
    ${COMP}/otm8009a
    ${COMP}/nt35510
    ${COMP}/adv7533
    ${COMP}/ft6x06
    ${FONTS}
)

# QSPI app: make board.c's SystemClock_Config()/MPU a no-op.
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE QSPI_APP=1)

# HAL modules used by the DSI/LTDC LCD + SDRAM framebuffer + I2C touch.
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dsi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_ltdc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_ltdc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma2d.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_sdram.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_ll_fmc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_i2c.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/qspi-flash-targets.cmake)

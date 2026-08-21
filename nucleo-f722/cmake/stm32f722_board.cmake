# Shared STM32F722ZE (NUCLEO-F722ZE) board layer for the benchmark projects.
# The F7 HAL + CMSIS come from the shared STM32Cube_FW_F7 package (see
# ../../../cmake/stm32cubef7.cmake) - every board in this repo uses the same
# one, no per-board copy. The board support (clock init to 216 MHz core /
# 216 MHz HCLK from the 8 MHz HSE bypass, USART3 console, SWV/ITM enable,
# newlib stubs, startup, linker script) lives in THIS board's own folder and is
# attached with stm32f722_apply_board().
#
# Usage (from a project CMakeLists.txt, after add_executable()):
#   include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32f722_board.cmake)
#   stm32f722_apply_board(${PROJECT_NAME}.elf "-Ofast")
#
# Requires the project to enable ASM (project(X C ASM)).

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/stm32cubef7.cmake)

set(NUCLEO_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/.." CACHE PATH
    "Root of the nucleo-f722 board tree (contains board/, cmake/, bare/)")

set(BOARD_DIR ${NUCLEO_ROOT}/board)

# Linker script for the STM32F722ZETx build (512 KB flash, DTCM/SRAM RW/ZI).
set(F722_LINKER_SCRIPT "${BOARD_DIR}/stm32f722xx.ld" CACHE FILEPATH
    "Linker script for the STM32F722 build")

# System init source. Defaults to the board copy of system_stm32f7xx.c (resets
# the RCC clock tree at startup - correct for a firmware that owns the clocks).
set(F722_SYSTEM_SOURCE "${BOARD_DIR}/system_stm32f7xx.c" CACHE FILEPATH
    "System init source for the STM32F722 build")

function(stm32f722_apply_board TGT OPT)
    separate_arguments(OPT_LIST NATIVE_COMMAND "${OPT}")

    if(STM32_ARMCLANG)
        set(_WARN_FLAGS -Wall)
    else()
        set(_WARN_FLAGS
            -Wall
            -Wno-unused-but-set-variable -Wno-unused-function
            -Wno-unused-variable -Wno-unused-parameter -Wno-maybe-uninitialized)
    endif()

    target_sources(${TGT} PRIVATE
        ${BOARD_DIR}/board.c
        ${BOARD_DIR}/swv_printf.c
        ${BOARD_DIR}/uart_printf.c
        ${BOARD_DIR}/syscalls.c
        ${BOARD_DIR}/startup_stm32f722xx.s
        ${F722_SYSTEM_SOURCE}
        # Shared F7 HAL from the STM32Cube_FW_F7 package.
        ${F7_HAL}/Src/stm32f7xx_hal.c
        ${F7_HAL}/Src/stm32f7xx_hal_cortex.c
        ${F7_HAL}/Src/stm32f7xx_hal_flash.c
        ${F7_HAL}/Src/stm32f7xx_hal_flash_ex.c
        ${F7_HAL}/Src/stm32f7xx_hal_gpio.c
        ${F7_HAL}/Src/stm32f7xx_hal_pwr.c
        ${F7_HAL}/Src/stm32f7xx_hal_pwr_ex.c
        ${F7_HAL}/Src/stm32f7xx_hal_rcc.c
        ${F7_HAL}/Src/stm32f7xx_hal_rcc_ex.c
        ${F7_HAL}/Src/stm32f7xx_hal_uart.c
        ${F7_HAL}/Src/stm32f7xx_hal_uart_ex.c
        ${F7_HAL}/Src/stm32f7xx_hal_dma.c
    )

    target_include_directories(${TGT} PRIVATE
        ${BOARD_DIR}
        ${F7_HAL}/Inc
        ${F7_HAL}/Inc/Legacy
        ${F7_CMSDEV}/Include
        ${F7_CMSIS}/Include
    )

    if(STM32_ARMCLANG)
        target_include_directories(${TGT} SYSTEM PRIVATE
            "${ARM_GCC_ROOT}/arm-none-eabi/include"
        )
    endif()

    target_compile_definitions(${TGT} PRIVATE
        STM32F722xx USE_HAL_DRIVER)

    target_compile_options(${TGT} PRIVATE
        -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16
        ${OPT_LIST} -g
        -ffunction-sections -fdata-sections ${_WARN_FLAGS}
    )

    set_target_properties(${TGT} PROPERTIES
        LINK_FLAGS "-mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 ${OPT} -Wl,--gc-sections -nostartfiles -Wl,-Map=${PROJECT_NAME}.map -T ${F722_LINKER_SCRIPT} -lc -lm"
    )

    # Silence newlib/libgcc's benign enum/stack-note warnings (see disco cmake).
    set_property(TARGET ${TGT} APPEND_STRING PROPERTY
        LINK_FLAGS " -Wl,--no-enum-size-warning -Wl,--no-wchar-size-warning -Wl,--no-warn-execstack")

    if(STM32_LTO AND NOT STM32_ARMCLANG)
        target_compile_options(${TGT} PRIVATE -flto)
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY LINK_FLAGS " -flto")
        set_source_files_properties(${BOARD_DIR}/syscalls.c PROPERTIES
            COMPILE_OPTIONS "-fno-lto")
    endif()
endfunction()
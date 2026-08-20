# Shared STM32F769NI (STM32F769I-Discovery) board layer for the benchmark
# projects. The board support (clock init to 216 MHz core / 216 MHz HCLK from
# the 25 MHz HSE, USART1 console, SWV/ITM enable, newlib stubs, startup, linker
# script) plus the STM32F7 HAL driver sources are attached to a target with
# stm32f769_apply_board().
#
# Usage (from a project CMakeLists.txt, after add_executable()):
#   include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/stm32f769_board.cmake)
#   stm32f769_apply_board(${PROJECT_NAME}.elf "-Ofast")
#
# Requires the project to enable ASM (project(X C ASM)).

set(F769_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/.." CACHE PATH
    "Root of the disco board tree (contains board/, cmake/, drivers/)")

set(BOARD_DIR ${F769_ROOT}/board)
set(F769_DRV ${F769_ROOT}/drivers)
set(F769_HAL ${F769_DRV}/STM32F7xx_HAL_Driver)
set(F769_CMSIS ${F769_DRV}/CMSIS)
set(F769_CMSDEV ${F769_CMSIS}/Device/ST/STM32F7xx)

# Linker script for the STM32F769NI build (2 MB flash, DTCM RW/ZI).
set(F769_LINKER_SCRIPT "${BOARD_DIR}/stm32f769ni.ld" CACHE FILEPATH
    "Linker script for the STM32F769 build")

# System init source. Defaults to the board copy of system_stm32f7xx.c (resets
# the RCC clock tree at startup - correct for a firmware that owns the clocks).
set(F769_SYSTEM_SOURCE "${BOARD_DIR}/system_stm32f7xx.c" CACHE FILEPATH
    "System init source for the STM32F769 build")

function(stm32f769_apply_board TGT OPT)
    separate_arguments(OPT_LIST NATIVE_COMMAND "${OPT}")

    # GCC-only warning switches; keep clang (armclang) clean.
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
        ${BOARD_DIR}/startup_stm32f769xx.s
        ${F769_SYSTEM_SOURCE}
        ${F769_HAL}/Src/stm32f7xx_hal.c
        ${F769_HAL}/Src/stm32f7xx_hal_cortex.c
        ${F769_HAL}/Src/stm32f7xx_hal_flash.c
        ${F769_HAL}/Src/stm32f7xx_hal_flash_ex.c
        ${F769_HAL}/Src/stm32f7xx_hal_gpio.c
        ${F769_HAL}/Src/stm32f7xx_hal_pwr.c
        ${F769_HAL}/Src/stm32f7xx_hal_pwr_ex.c
        ${F769_HAL}/Src/stm32f7xx_hal_rcc.c
        ${F769_HAL}/Src/stm32f7xx_hal_rcc_ex.c
        ${F769_HAL}/Src/stm32f7xx_hal_uart.c
        ${F769_HAL}/Src/stm32f7xx_hal_uart_ex.c
        # On-board SDRAM (board.c inits it for every project).
        ${F769_HAL}/Src/stm32f7xx_hal_sdram.c
        ${F769_HAL}/Src/stm32f7xx_ll_fmc.c
        ${F769_HAL}/Src/stm32f7xx_hal_dma.c
        ${F769_ROOT}/vendor/Drivers/BSP/STM32F769I-Discovery/stm32f769i_discovery_sdram.c
    )

    target_include_directories(${TGT} PRIVATE
        ${BOARD_DIR}
        ${F769_HAL}/Inc
        ${F769_HAL}/Inc/Legacy
        ${F769_CMSDEV}/Include
        ${F769_CMSIS}/Include
        ${F769_ROOT}/vendor/Drivers/BSP/STM32F769I-Discovery
    )

    # armclang has no bundled libc headers: point it at the GNU newlib include
    # dir so <stdio.h>/<string.h>/... resolve to the same newlib we link.
    if(STM32_ARMCLANG)
        target_include_directories(${TGT} SYSTEM PRIVATE
            "${ARM_GCC_ROOT}/arm-none-eabi/include"
        )
    endif()

    target_compile_definitions(${TGT} PRIVATE
        STM32F769xx USE_HAL_DRIVER)

    target_compile_options(${TGT} PRIVATE
        -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16
        ${OPT_LIST} -g
        -ffunction-sections -fdata-sections ${_WARN_FLAGS}
    )

    set_target_properties(${TGT} PROPERTIES
        LINK_FLAGS "-mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 ${OPT} -Wl,--gc-sections -nostartfiles -Wl,-Map=${PROJECT_NAME}.map -T ${F769_LINKER_SCRIPT} -lc -lm"
    )

    # newlib/libgcc's thumb/v7e-m+fp multilib objects are built with
    # -fshort-enums and lack .note.GNU-stack, so a normal link spews ~60
    # benign warnings. Silence them (the sizes match the ARM EABI defaults
    # our objects use, so this is noise, not an ABI error):
    set_property(TARGET ${TGT} APPEND_STRING PROPERTY
        LINK_FLAGS " -Wl,--no-enum-size-warning -Wl,--no-wchar-size-warning -Wl,--no-warn-execstack")

    # Link-time optimization (GCC only). armclang -flto emits LLVM bitcode
    # (.llvm.lto) that GNU ld cannot consume, so STM32_LTO is ignored there.
    if(STM32_LTO AND NOT STM32_ARMCLANG)
        target_compile_options(${TGT} PRIVATE -flto)
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY LINK_FLAGS " -flto")
        # GCC LTO loses the newlib syscall-stub definitions (_fstat/_isatty/
        # _kill/_getpid) that live in syscalls.c: the plugin fails to resolve
        # the THM_CALL relocations from libc.a against the LTO IR, leaving
        # "undefined reference / Unknown destination type (ARM/Thumb)".
        # syscalls.c is a tiny retarget layer (not benchmark code), so compile
        # it without LTO.
        set_source_files_properties(${BOARD_DIR}/syscalls.c PROPERTIES
            COMPILE_OPTIONS "-fno-lto")
    endif()
endfunction()

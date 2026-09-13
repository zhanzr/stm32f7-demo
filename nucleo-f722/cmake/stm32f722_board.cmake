# Shared STM32F722ZE (NUCLEO-F722ZE) board layer for the benchmark projects.
# The F7 HAL + CMSIS come from the VENDORED repo-root `drivers/` folder (see
# ../../cmake/stm32cubef7.cmake and drivers/README.md; override with
# -DSTM32F7_HAL_ROOT= to use a full STM32Cube_FW_F7 package's Drivers dir
# instead). The board support (clock init to 216 MHz core / 216 MHz HCLK from
# the 8 MHz HSE bypass, USART3 console, SWV/ITM enable, newlib stubs, startup,
# linker script) lives in THIS board's own folder and is attached with
# stm32f722_apply_board().
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

    # GCC-only warning switches; keep clang-based toolchains clean.
    if(STM32_ARMCLANG OR STM32_STARM_CLANG)
        set(_WARN_FLAGS -Wall -Wno-unused-command-line-argument)
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
        # Vendored F7 HAL (repo-root drivers/ folder).
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

    # armclang has no bundled libc headers: point it at the GNU newlib include
    # dir so <stdio.h>/<string.h>/... resolve to the same newlib we link.
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

    # starm-clang links with LLD by default; use -Xlinker for linker flags.
    # ST's clang runtime has no thumbv7e-m + fpv5-sp-d16 multilib, so the
    # hard-float single-precision libraries come from the thumbv8m.main hard
    # fpv5-sp-d16 multilib (same EABI; no v8-M-only instructions - see
    # starm-clang-toolchain.cmake).
    if(STM32_STARM_CLANG)
        set(_STARM_SYSROOT "${STARM_ROOT}/lib/clang-runtimes/newlib")
        set(_STARM_LIBDIR "${_STARM_SYSROOT}/arm-none-eabi/armv8m.main_hard_fp_exn_rtti_unaligned_size/lib")
        set(_LDFLAGS "-nostartfiles")
        set(_LDFLAGS "${_LDFLAGS} -Xlinker -T -Xlinker ${F722_LINKER_SCRIPT}")
        set(_LDFLAGS "${_LDFLAGS} -Xlinker --gc-sections")
        set(_LDFLAGS "${_LDFLAGS} -Xlinker -Map=${PROJECT_NAME}.map")
        set(_LDFLAGS "${_LDFLAGS} -L${_STARM_LIBDIR}")
        set(_LDFLAGS "${_LDFLAGS} -Xlinker --start-group")
        set(_LDFLAGS "${_LDFLAGS} ${_STARM_LIBDIR}/libclang_rt.builtins.a -lc -lm")
        set(_LDFLAGS "${_LDFLAGS} -Xlinker --end-group")
    else()
        set(_LDFLAGS "-mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 ${OPT}")
        set(_LDFLAGS "${_LDFLAGS} -Wl,--gc-sections -nostartfiles")
        set(_LDFLAGS "${_LDFLAGS} -Wl,-Map=${PROJECT_NAME}.map")
        set(_LDFLAGS "${_LDFLAGS} -T ${F722_LINKER_SCRIPT}")
        set(_LDFLAGS "${_LDFLAGS} -lc -lm")
    endif()

    set_target_properties(${TGT} PROPERTIES
        LINK_FLAGS "${_LDFLAGS}"
    )

    # Silence newlib/libgcc's benign enum/stack-note warnings (see disco cmake).
    # LLD (starm-clang) does not support these GNU-ld-only switches.
    if(NOT STM32_STARM_CLANG)
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Wl,--no-enum-size-warning -Wl,--no-wchar-size-warning -Wl,--no-warn-execstack")
    endif()

    # Link-time optimization (GCC and starm-clang).
    # GCC: -flto via GNU ld's lto plugin.
    # starm-clang: -flto=full via LLD's native LTO (bitcode consumed directly).
    # Keil armclang: LTO impossible — emits LLVM bitcode that GNU ld cannot use.
    if(STM32_LTO AND STM32_STARM_CLANG)
        # Full LLVM LTO: compile to bitcode (-flto=full), link with LLD's LTO.
        # Keep -ffat-lto-objects so syscalls.c-style -fno-lto sources still
        # produce relocatable objects. The aggressive pass thresholds come from
        # ST's OmaxLTO.cfg. C++-only devirt flags are omitted (pure-C benchmarks).
        target_compile_options(${TGT} PRIVATE
            -flto=full -ffat-lto-objects
        )
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -flto=full")
        # Pass aggressive LTO plugin opts (from ST's OmaxLTO.cfg).
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-extra-LTO-loop-unroll=true")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-inline-threshold=500")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-unroll-threshold=450")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-unroll-partial-threshold=450")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-unroll-max-iteration-count-to-analyze=20")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-lsr-complexity-limit=1073741823")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-force-attribute=main:norecurse")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-enable-dfa-jump-thread")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-enable-loop-flatten")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-enable-unroll-and-jam")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-enable-inline-memcpy-ld-st")
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Xlinker -plugin-opt=-enable-loop-versioning-licm")
        # syscalls.c: tiny retarget layer, compile without LTO to avoid
        # newlib syscall-stub resolution issues (same rationale as GCC).
        set_source_files_properties(${BOARD_DIR}/syscalls.c PROPERTIES
            COMPILE_OPTIONS "-fno-lto")
    elseif(STM32_LTO AND NOT STM32_ARMCLANG AND NOT STM32_STARM_CLANG)
        target_compile_options(${TGT} PRIVATE -flto)
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY LINK_FLAGS " -flto")
        # GCC LTO loses the newlib syscall-stub definitions (_fstat/_isatty/
        # _kill/_getpid) that live in syscalls.c; compile it without LTO.
        set_source_files_properties(${BOARD_DIR}/syscalls.c PROPERTIES
            COMPILE_OPTIONS "-fno-lto")
    endif()
endfunction()

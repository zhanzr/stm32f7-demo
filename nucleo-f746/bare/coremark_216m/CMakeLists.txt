# CoreMark 1.0.1 benchmark for the STM32F746ZG (NUCLEO-F746ZG) @ 216 MHz.
# Default compiler is GNU arm-none-eabi-gcc at the most aggressive optimization
# (-Ofast -ffp-contract=fast -funroll-all-loops) + single-precision FPU
# (fpv5-sp-d16).  Additional toolchains:
#   armclang   â€?Keil Arm Compiler 6, C + GNU as/ld
#   starm-clang â€?ST's LLVM 21 (starm-clang) + LLD; supports full LTO
#
# Tuning note (measured on the nano-f411 F411, same sources): -funroll-all-loops
# beat -funroll-loops by ~5% on GCC; armclang is flat across -O3/-Ofast/unroll
# and only moves at -Omax. Verify on this board with the results in README.md.
#
# Pico-style workflow:
#   mkdir -p build && cd build
#   cmake -G Ninja ..                        # gcc (default)
#   cmake -G Ninja -DSTM32_TOOLCHAIN=armclang ..       # Keil AC6
#   cmake -G Ninja -DSTM32_TOOLCHAIN=starm-clang ..   # ST LLVM + LTO
#   cmake -G Ninja -DSTM32_LTO=ON ..        # enable LTO (gcc or starm-clang)
#   ninja           # builds .elf / .hex / .bin
#   ninja flash     # programs the board (probe-rs, ST-Link / SWD)
#   ninja bin       # optional raw .bin image, "in case" you need it

cmake_minimum_required(VERSION 3.13)

# We pass the cpu/arch flags ourselves (-mcpu=...) and link with GNU ld, so
# keep CMake's ARMClang module from auto-adding --cpu/--march flags.
if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

# Toolchain selection. gcc (default) = GNU arm-none-eabi end-to-end;
# armclang = Keil Arm Compiler 6 C + GNU as/ld;
# starm-clang = ST LLVM 21 C + LLD (supports full LLVM LTO).
# CMAKE_TOOLCHAIN_FILE is cached, so use a separate build dir per toolchain.
set(STM32_TOOLCHAIN "gcc" CACHE STRING "C compiler: gcc (default), armclang (Keil AC6), or starm-clang (ST LLVM)")
set_property(CACHE STM32_TOOLCHAIN PROPERTY STRINGS gcc armclang starm-clang)

# Link-time optimization. GCC: -flto; starm-clang: -flto=full via LLD.
# armclang (Keil): LTO impossible â€?emits LLVM bitcode that GNU ld cannot use.
# armclang '-Omax' also emits such bitcode without explicit LTO; pass
# '-fno-lto' alongside it (see BENCH_OPT_C) to restore normal ELF objects.
option(STM32_LTO "Enable LTO (gcc or starm-clang only)" OFF)

if(NOT CMAKE_TOOLCHAIN_FILE)
    if(STM32_TOOLCHAIN STREQUAL "gcc")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
    elseif(STM32_TOOLCHAIN STREQUAL "armclang")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-keil-toolchain.cmake")
    else()
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/starm-clang-toolchain.cmake")
    endif()
endif()

project(coremark_216m C ASM)

# armclang only: restore the GNU-style link rule after project() (CMake's
# ARMClang module installs an armlink-style rule that GNU ld cannot use).
if(STM32_TOOLCHAIN STREQUAL "armclang")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-postproject.cmake)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(BENCH_OPT "-Ofast -ffp-contract=fast -funroll-all-loops" CACHE STRING
    "Optimization flags for stm32f746_apply_board (e.g. -Omax for armclang)")

# C-language-only optimization flags, applied to the C sources but NOT to the
# GNU-as startup/toolchain files or the link step. Required for armclang-only
# options that arm-none-eabi-gcc rejects (e.g. '-Omax').
set(BENCH_OPT_C "" CACHE STRING
    "Extra C-language-only optimization flags (e.g. armclang '-Omax'; not passed to ASM/link)")
set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/coremark_1_0_1)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/utils.c
    src/core_portme.c
    ${CORE}/core_list_join.c
    ${CORE}/core_main.c
    ${CORE}/core_matrix.c
    ${CORE}/core_state.c
    ${CORE}/core_util.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32f746_board.cmake)
stm32f746_apply_board(${PROJECT_NAME}.elf "${BENCH_OPT}")

if(BENCH_OPT_C)
    separate_arguments(BENCH_OPT_C_LIST NATIVE_COMMAND "${BENCH_OPT_C}")
    target_compile_options(${PROJECT_NAME}.elf PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:${BENCH_OPT_C_LIST}>")
endif()

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CORE}
)

# Display label for the results banner (src/core_portme.h FLAGS_STR holds the
# GCC-default text). Set it when the actual flags differ, e.g. the armclang
# -Omax recipe:  -DCOMPILER_FLAGS_LABEL="-Omax -fno-lto"
set(COMPILER_FLAGS_LABEL "" CACHE STRING
    "Override for CoreMark's displayed COMPILER_FLAGS label")
if(COMPILER_FLAGS_LABEL)
    target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
        "COMPILER_FLAGS=\"${COMPILER_FLAGS_LABEL}\"")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)

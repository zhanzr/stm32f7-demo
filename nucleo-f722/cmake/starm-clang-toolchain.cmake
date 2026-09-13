# CMake toolchain file for ST's Arm Clang (starm-clang) + LLD.
#
# starm-clang is STMicroelectronics' LLVM/Clang 21 toolchain shipped with
# STM32CubeIDE.  Unlike Keil armclang + GNU ld, starm-clang pairs with
# LLVM's own linker (LLD) which can consume LLVM bitcode, enabling full
# cross-TU LTO (-flto=full).
#
# C compilation: starm-clang (LLVM)
# Assembly:      GNU arm-none-eabi-gcc (for GAS-syntax startup_stm32f722xx.s)
# Linking:       starm-clang driver -> ld.lld (LLVM LLD)
#
# Multilib note (STM32F722ZE, Cortex-M7, single-precision FPU): ST's clang
# runtime has no thumbv7e-m + fpv5-sp-d16 multilib, so the hard-float ABI
# libraries come from the thumbv8m.main hard fpv5-sp-d16 multilib
# (armv8m.main_hard_fp_exn_rtti_unaligned_size) - same hard single-precision
# EABI, and its objects use no v8-M-only instructions (verified by
# disassembling libc.a/libm.a/libclang_rt.builtins.a), so they run on the M7.
#
# Selection (project CMakeLists):
#   set(STM32_TOOLCHAIN "starm-clang" CACHE STRING "...")
# or from the command line:
#   cmake -G Ninja -DSTM32_TOOLCHAIN=starm-clang ..
#
# Variables:
#   STARM_ROOT : ST Arm Clang tools root (contains bin/starm-clang.exe)

cmake_minimum_required(VERSION 3.13)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m7)

# CMake 3.30+ ARMClang support: suppress auto-added --cpu/--march flags.
if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

set(STARM_ROOT "D:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.llvm.win32_1.0.200.202603311046/tools" CACHE PATH
    "ST Arm Clang tools root (contains bin/starm-clang.exe)")

set(ARM_GCC_ROOT "D:/Arm/GNU Toolchain mingw-w64-x86_64-arm-none-eabi" CACHE PATH
    "GNU arm-none-eabi root (assembler, linker, newlib)")

# C compiler: starm-clang with LLVM's newlib sysroot.
set(CMAKE_C_COMPILER "${STARM_ROOT}/bin/starm-clang.exe")

# Assembly: GNU as (startup_stm32f722xx.s uses GAS syntax).
set(CMAKE_ASM_COMPILER "${ARM_GCC_ROOT}/bin/arm-none-eabi-gcc.exe")

# Bare-metal target: compile-only sanity check during configure.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Object tools (ELF output from LLD link).
set(CMAKE_OBJCOPY "${STARM_ROOT}/bin/starm-objcopy.exe")
set(CMAKE_OBJDUMP "${STARM_ROOT}/bin/starm-objdump.exe")
set(CMAKE_SIZE "${STARM_ROOT}/bin/starm-size.exe")
set(CMAKE_AR "${STARM_ROOT}/bin/starm-ar.exe")

# Tells cmake/stm32f722_board.cmake to apply starm-clang-specific tweaks.
set(STM32_STARM_CLANG TRUE)

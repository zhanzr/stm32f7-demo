# CoreMark 1.0 @ 216 MHz - STM32F722ZE (NUCLEO-F722ZE)

EEMBC CoreMark 1.0 (stock `coremark_1_0_1` sources), **25,000 iterations**, on
the NUCLEO-F722ZE board (STM32F722ZE) clocked at **216 MHz** (HSE 8 MHz from
the ST-Link MCO in bypass mode, PLL M=8 N=432 P=2 -> SYSCLK 216 MHz, HCLK
216 MHz, APB1 54 MHz, APB2 108 MHz, OverDrive on - clock tree copied verbatim
from the vendor STM32F722ZE-Nucleo template). Same sources as the disco-f769
benchmark; only the board layer differs. Compiler-agnostic: the same sources
build with **GNU arm-none-eabi-gcc**, **Keil Arm Compiler 6 (armclang)** or
**ST Arm clang** (starm-clang), selected at configure time. The port uses the
HAL SysTick 1 kHz tick (`HAL_GetTick()`) for CORE_TICKS, so it works
identically on all three compilers.

## Results (measured on hardware, 216 MHz, single-precision FPU, I/D caches on)

| Toolchain     | Flags                                                 | Iterations/s | Total time |
| ------------- | ------------------------------------------------------ | ------------ | ---------- |
| GCC 15.3.1    | `-Ofast -ffp-contract=fast -funroll-all-loops`         | 941.66       | 26.55 s    |
| GCC + LTO     | same + `-flto`                                         | 874.83       | 28.58 s    |
| ARMCLANG 6.24 | `-Ofast -ffp-contract=fast -funroll-all-loops`         | 916.19       | 27.29 s    |
| ARMCLANG 6.24 | `-Omax -fno-lto`                                       | **1075.82**  | **23.24 s**|
| ST Arm clang  | `-Ofast -ffp-contract=fast -funroll-all-loops`         | 792.77       | 31.54 s    |

Measured on hardware: every configuration printed **`Correct operation
validated.`** with the expected CRCs (seedcrc 0xe9f5, crcfinal 0xcc42).

The heap is in the on-chip SRAM1 (the `_sbrk` heap of this board layer), vs
the disco-f769's external SDRAM; CoreMark's malloc'd list is a small part of
the workload, so the scores are essentially the same as the disco-f769's
(same core, both at 216 MHz). The best measured 1075.82 matches ST's
published ~1073 CoreMark for a Cortex-M7 @ 216 MHz.

## Most aggressive flags

- **ARMCLANG (Keil AC6): `-Omax -fno-lto`** - **1075.82 iterations/s**, +17%
  over the `-Ofast`-class flags (armclang is flat across -O3/-Ofast/unroll at
  ~916). Bare `-Omax` makes armclang emit **LLVM LTO objects** that GNU ld
  cannot link, hence `-fno-lto`; pass it as a **C-only** flag (`BENCH_OPT_C`,
  kept off the asm/link steps) with `BENCH_OPT` cleared.
- **GCC:** `-Ofast -ffp-contract=fast -funroll-all-loops` (the default) →
  941.66 it/s; `-funroll-all-loops` beats the old `-funroll-loops` build by
  ~1.9% (924.35). **Do not add `-flto`** - it *loses* ~7% here (874.83).
- **ST Arm clang:** 792.77 it/s at the default flags; flat across the tuning
  flags tried.

```bash
cmake -G Ninja ..                                                    # gcc (default)
cmake -G Ninja -DSTM32_LTO=ON ..                                     # gcc + LTO (slower here)
cmake -G Ninja -DSTM32_TOOLCHAIN=armclang ..                         # Keil AC6
cmake -G Ninja -DSTM32_TOOLCHAIN=armclang '-DBENCH_OPT=' '-DBENCH_OPT_C=-Omax -fno-lto' ..  # AC6 best
cmake -G Ninja -DSTM32_TOOLCHAIN=starm-clang ..                      # ST LLVM
```

## Build

Requires the CMake/Ninja environment (MSYS2 mingw64, `build.sh` adds it to
`PATH` automatically). Use a **separate build dir per toolchain** because
`CMAKE_TOOLCHAIN_FILE` is cached after configure.

```bash
bash build.sh                      # == cmake -G Ninja .. && ninja  (GCC)

# armclang (optional)
cmake -G Ninja -B build-ac6 -S . -DSTM32_TOOLCHAIN=armclang ..
cmake --build build-ac6

# armclang at -Omax (best measured: 1075.82 it/s):
# BENCH_OPT_C is applied to the C files only (arm-none-eabi-gcc rejects -Omax
# at the asm/link step), and -fno-lto is required because -Omax would
# otherwise emit LTO objects GNU ld cannot link.
cmake -G Ninja -B build-ac6-omax -S . -DSTM32_TOOLCHAIN=armclang '-DBENCH_OPT=' '-DBENCH_OPT_C=-Omax -fno-lto' ..
cmake --build build-ac6-omax

# starm-clang (optional)
cmake -G Ninja -B build-starm -S . -DSTM32_TOOLCHAIN=starm-clang ..
cmake --build build-starm
```

Optimization flags are the `BENCH_OPT` cache variable; the displayed result
banner can be relabeled with `-DCOMPILER_FLAGS_LABEL="..."` when it differs
from the GCC-default text.

`ninja` builds the `.elf` + `.hex` (the `.hex` is what `ninja flash` programs);
`ninja bin` additionally writes a raw `.bin` image "in case" you need it.

## Flash & measure

```bash
ninja flash        # probe-rs through the on-board ST-Link (SWD)
```

If several ST-Links are attached, pin the nucleo probe at configure time:
`cmake -G Ninja -DDEBUG_PROBE=0483:3752:xxxx... ..`.

Open the **USART3** console (PD8/PD9, `COMxx` @ 115200 via the ST-Link VCP).
Capture at least ~65 s so two full (~27 s) runs complete and the final
`CoreMark 1.0 : <score> / <compiler> / Static` line is printed.

## Notes

* **SysTick**: `board.c` defines `SysTick_Handler` -> `HAL_IncTick()` (same
  requirement as the Dhrystone port).
* **ITERATIONS**: 25,000 - at 216 MHz a run takes ~23-32 s depending on the
  toolchain (CoreMark rejects runs shorter than 10 s).
* Port uses `SEED_VOLATILE` (fixed volatile seeds, so the known-CRC validation
  still matches), `MEM_LOCATION "Static"`, `HAS_FLOAT 1`, and the CORE_TICKS
  timer is `HAL_GetTick()` (1 ms SysTick).
* **FPU**: single-precision only (SFPU) - `-mfpu=fpv5-sp-d16`; `float` math
  uses the FPU, `double` falls back to software.
* **starm-clang**: ST's clang runtime has no thumbv7e-m + fpv5-sp-d16
  multilib, so the board layer links the hard-float single-precision libs
  from the thumbv8m.main multilib (same EABI; see
  `../../cmake/starm-clang-toolchain.cmake`), and `board/syscalls.c` provides
  the `__aeabi_read_tp` thread-pointer stub starm-clang's newlib `errno`
  needs.
* Console: USART3 (PD8/PD9, AF7) via the on-board ST-Link VCP.

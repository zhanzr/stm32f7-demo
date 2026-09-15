# Dhrystone 2.1 @ 216 MHz - STM32F746ZG (NUCLEO-F746ZG)

Classic Dhrystone 2.1 (dhry_1.c / dhry_2.c / dhry.h), **12,000,000 runs**, on
the NUCLEO-F746ZG board (STM32F746ZG) clocked at **216 MHz** (HSE 8 MHz from
the ST-Link MCO in bypass mode, PLL M=8 N=432 P=2 -> SYSCLK 216 MHz, HCLK
216 MHz, APB1 54 MHz, APB2 108 MHz, OverDrive on - clock tree copied verbatim
from the vendor STM32F746ZG-Nucleo template). Same sources as the other
boards' benchmarks; only the board layer differs. Compiler-agnostic: the same
sources build with either **GNU arm-none-eabi-gcc** or **Keil Arm Compiler 6
(armclang)**, selected at configure time.

## Results (measured on hardware, 216 MHz, single-precision FPU, I/D caches on)

| Toolchain    | Flags                                      | Dhrystones/s | DMIPS/MHz |
| ------------ | ------------------------------------------ | ------------ | --------- |
| GCC 15.3.1   | `-Ofast -ffp-contract=fast -funroll-loops` | 972,841      | 2.563     |
| ARMCLANG 6.24| `-Ofast -ffp-contract=fast -funroll-loops` | **1,045,843**| **2.756** |

All runs print correct final values (Int_Glob=5, Arr_2_Glob = runs+10, ...).
One run takes ~12 s, comfortably above the 2 s `Too_Small_Time` gate.
`printf("%f")` (used for the score lines) works via the single-precision FPU
build (`-mfpu=fpv5-sp-d16`) - see the board README.

Identical scores to the nucleo-f722 (2.563 / 2.756) - same Cortex-M7 core at
216 MHz with the heap in on-chip SRAM, so the numbers repeat within noise.
GCC on the F746 measured 955,642 (2.518) and 972,841 (2.563) on consecutive
runs; the table records the second run.

> The heap (where Dhrystone's `Ptr_Glb` / `Next_Ptr_Glb` records live) is in
> the **on-chip SRAM1** (the `_sbrk` heap of this board layer) - unlike the
> disco-f769, whose malloc'd working set sits in the external SDRAM
> (write-through). That is why this board scores **much higher** than the
> disco-f769's 1.370 DMIPS/MHz with the same sources and flags.

> ! **Do not use LTO for Dhrystone.** GCC `-flto` sees the whole program and
> hoists loop-invariant work out of the timed loop, inflating the score (a
> known GCC artifact, not a real measurement).

## Most aggressive flags

- **ARMCLANG (Keil AC6):** `-Ofast -ffp-contract=fast -funroll-loops` is the
  best measured recipe - **1,045,843 Dhrystones/s (2.756 DMIPS/MHz)**, +7.5%
  over GCC.
- **GCC:** `-Ofast -ffp-contract=fast -funroll-loops` (the default) →
  972,841 Dhrystones/s (2.563 DMIPS/MHz); **do not** add `-flto` (inflates
  the score - an artifact, see the note above).
- **ST Arm clang:** not supported for Dhrystone on this board (gcc/armclang
  only).

```bash
cmake -G Ninja ..                                              # gcc (default)
cmake -G Ninja -DSTM32_TOOLCHAIN=armclang ..                   # Keil AC6
```

## Build

Requires the CMake/Ninja environment (MSYS2 mingw64, `build.sh` adds it to
`PATH` automatically). Use a **separate build dir per toolchain** because
`CMAKE_TOOLCHAIN_FILE` is cached after configure.

```bash
bash build.sh                      # == cmake -G Ninja .. && ninja  (GCC)

# Keil AC6 (optional)
cmake -G Ninja -B build-ac6 -S . -DSTM32_TOOLCHAIN=armclang ..
cmake --build build-ac6
```

Optimization flags are the `BENCH_OPT` cache variable (default
`-Ofast -ffp-contract=fast -funroll-loops`); `BENCH_OPT_C` adds C-only flags
(e.g. armclang `-Omax`, kept off the asm/link steps).

`ninja` builds the `.elf` + `.hex` (the `.hex` is what `ninja flash` programs);
`ninja bin` additionally writes a raw `.bin` image "in case" you need it.

## Flash & measure

```bash
ninja flash        # probe-rs through the on-board ST-Link (SWD)
```

If several ST-Links are attached, pin the nucleo probe at configure time:
`cmake -G Ninja -DDEBUG_PROBE=0483:3752:xxxx... ..`.

Open the **USART3** console (PD8/PD9, `COMxx` @ 115200 via the ST-Link VCP).
The console prints the Dhrystones/s and DMIPS/MHz lines every ~10 s; capture a
few seconds longer than one full run to get a clean result line.

## Notes

* **SysTick**: `board.c` defines `SysTick_Handler` -> `HAL_IncTick()` (without
  it the core wedges in the startup weak handler on the first tick).
* **RUN_NUMBER**: kept at 12,000,000 - at 216 MHz a run takes ~12 s.
* **FPU**: single-precision only (SFPU) - `-mfpu=fpv5-sp-d16`; `float` math
  uses the FPU, `double` (e.g. newlib's `%f`) falls back to software.
* **armclang printf shim**: armclang (ARMCLIB mode) specializes `printf` into
  `__2printf` + hidden `_printf_*` helpers that GNU ld + newlib cannot
  resolve; `cmake/printf_rename.h` renames `printf` -> `bench_printf` (a
  `vprintf` wrapper in `board/uart_printf.c`).
* Console: USART3 (PD8/PD9, AF7) via the on-board ST-Link VCP.

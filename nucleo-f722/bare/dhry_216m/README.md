# Dhrystone 2.1 @ 216 MHz - STM32F722ZE (NUCLEO-F722ZE)

Classic Dhrystone 2.1 (dhry_1.c / dhry_2.c / dhry.h), **12,000,000 runs**, on
the NUCLEO-F722ZE board (STM32F722ZE) clocked at **216 MHz** (HSE 8 MHz from
the ST-Link MCO in bypass mode, PLL M=8 N=432 P=2 -> SYSCLK 216 MHz, HCLK
216 MHz, APB1 54 MHz, APB2 108 MHz, OverDrive on - clock tree copied verbatim
from the vendor STM32F722ZE-Nucleo template). Same sources as the disco-f769
benchmark; only the board layer differs.

## Results (measured on hardware, 216 MHz, single-precision FPU, I/D caches on)

| Toolchain  | Flags                                      | Dhrystones/s | DMIPS/MHz |
| ---------- | ------------------------------------------ | ------------ | --------- |
| GCC 15.3.1 | `-Ofast -ffp-contract=fast -funroll-loops` | 972,841      | 2.563     |

(Two consecutive runs measured 955,566 / 2.518 and 972,841 / 2.563; the table
records the second run.)

> The heap (where Dhrystone's `Ptr_Glb` / `Next_Ptr_Glb` records live) is in
> the **on-chip SRAM1** (the `_sbrk` heap of this board layer) - unlike the
> disco-f769, whose malloc'd working set sits in the external SDRAM
> (write-through). That is why this board scores **much higher** than the
> disco-f769's 1.370 DMIPS/MHz with the same sources and flags: on-chip SRAM
> beats the write-through external SDRAM for the record-heavy Dhrystone loop.

Measured on hardware: all final values correct (Int_Glob=5, Arr_2_Glob =
runs+10, ...), one run takes ~12 s, comfortably above the 2 s
`Too_Small_Time` gate. `printf("%f")` (used for the score lines) works via the
single-precision FPU build (`-mfpu=fpv5-sp-d16`) - see the board README.

> ! **Do not use LTO for Dhrystone.** GCC `-flto` sees the whole program and
> hoists loop-invariant work out of the timed loop, inflating the score (a
> known GCC artifact, not a real measurement).

## Build

Requires the CMake/Ninja environment (MSYS2 mingw64, `build.sh` adds it to
`PATH` automatically). Use a **separate build dir per toolchain** because
`CMAKE_TOOLCHAIN_FILE` is cached after configure.

```bash
bash build.sh                      # == cmake -G Ninja .. && ninja
```

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
* Console: USART3 (PD8/PD9, AF7) via the on-board ST-Link VCP.

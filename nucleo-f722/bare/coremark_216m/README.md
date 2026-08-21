# CoreMark 1.0 @ 216 MHz - STM32F722ZE (NUCLEO-F722ZE)

EEMBC CoreMark 1.0 (stock `coremark_1_0_1` sources), **25,000 iterations**, on
the NUCLEO-F722ZE board (STM32F722ZE) clocked at **216 MHz** (HSE 8 MHz from
the ST-Link MCO in bypass mode, PLL M=8 N=432 P=2 -> SYSCLK 216 MHz, HCLK
216 MHz, APB1 54 MHz, APB2 108 MHz, OverDrive on - clock tree copied verbatim
from the vendor STM32F722ZE-Nucleo template). Same sources as the disco-f769
benchmark; only the board layer differs.

## Results (measured on hardware, 216 MHz, single-precision FPU, I/D caches on)

| Toolchain  | Flags                                      | CoreMark 1.0 | Iterations/s | Total time |
| ---------- | ------------------------------------------ | ------------ | ------------ | ---------- |
| GCC 15.3.1 | `-Ofast -ffp-contract=fast -funroll-loops` | 924.35       | 924.35       | 27.05 s    |

Measured on hardware: the build printed **`Correct operation validated.`** with
the expected CRCs (seedcrc 0xe9f5, crcfinal 0xcc42).

The heap is in the on-chip SRAM1 (the `_sbrk` heap of this board layer), vs
the disco-f769's external SDRAM; CoreMark's malloc'd list is a small part of
the workload, so the score is essentially the same as the disco-f769's 932.28
(same sources and flags, both at 216 MHz).

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
Capture at least ~40 s so one full (~27 s) run completes and the final
`CoreMark 1.0 : <score> / <compiler> / Static` line is printed.

## Notes

* **SysTick**: `board.c` defines `SysTick_Handler` -> `HAL_IncTick()` (same
  requirement as the Dhrystone port).
* **ITERATIONS**: 25,000 - at 216 MHz a run takes ~27 s, valid (CoreMark
  rejects runs shorter than 10 s).
* Port uses `SEED_VOLATILE` (fixed volatile seeds, so the known-CRC validation
  still matches), `MEM_LOCATION "Static"`, `HAS_FLOAT 1`, and the CORE_TICKS
  timer is `HAL_GetTick()` (1 ms SysTick).
* **FPU**: single-precision only (SFPU) - `-mfpu=fpv5-sp-d16`; `float` math
  uses the FPU, `double` falls back to software.
* Console: USART3 (PD8/PD9, AF7) via the on-board ST-Link VCP.

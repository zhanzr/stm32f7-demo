# Dhrystone 2.1 @ 216 MHz - STM32F769NI (STM32F769I-Discovery)

Classic Dhrystone 2.1 (dhry_1.c / dhry_2.c / dhry.h), **12,000,000 runs**, on
the STM32F769I-Discovery board (STM32F769NI) clocked at **216 MHz** (HSE
25 MHz, PLL M=25 N=432 P=2 -> SYSCLK 216 MHz, HCLK 216 MHz, APB1 54 MHz,
APB2 108 MHz, OverDrive on - clock tree copied verbatim from the vendor 216 MHz
template). Compiler-agnostic: the same sources build with either
**GNU arm-none-eabi-gcc** or **armclang** (AC6 / the LLVM embedded toolchain),
selected at configure time.

## Results (measured on hardware, 216 MHz, hard-float, I/D caches on)

| Toolchain    | Flags                                      | Dhrystones/s | DMIPS/MHz |
| ------------ | ------------------------------------------ | ------------ | --------- |
| GCC 15.3.1   | `-Ofast -ffp-contract=fast -funroll-loops` | 519,998      | 1.370     |

> The heap (where Dhrystone's `Ptr_Glb` / `Next_Ptr_Glb` records live) is in
> the **on-board SDRAM** (write-through cacheable) - the shared board layer's
> two-region SDRAM heap. Every store to those records is a write-through to the
> SDRAM, which is why the score is lower than a DTCM-heap build (~2.72 DMIPS/MHz).
> The number is the honest cost of running the malloc'd working set in SDRAM.

Measured on hardware: all final values correct (Int_Glob=5, Arr_2_Glob =
runs+10, ...), one run takes ~23 s, comfortably above the 2 s `Too_Small_Time`
gate.

> ! **Do not use LTO for Dhrystone.** GCC `-flto` sees the whole program and
> hoists loop-invariant work out of the timed loop, inflating the score. The
> LTO number is meaningless and is **excluded from the table above** (a known
> GCC artifact, not a real measurement).

## Build

Requires the CMake/Ninja environment (MSYS2 mingw64, `build.sh` adds it to
`PATH` automatically). Use a **separate build dir per toolchain** because
`CMAKE_TOOLCHAIN_FILE` is cached after configure.

```bash
# GNU arm-none-eabi-gcc (default)
bash build.sh                      # == cmake -G Ninja .. && ninja

# Keil AC6 (armclang)
mkdir -p build-ac6 && cd build-ac6
cmake -G Ninja -DSTM32_TOOLCHAIN=armclang ..
ninja
```

`ninja` builds the `.elf` + `.hex` (the `.hex` is what `ninja flash` programs);
`ninja bin` additionally writes a raw `.bin` image "in case" you need it.

## Flash & measure

```bash
ninja flash        # probe-rs through the on-board ST-Link V2 (SWD)
```

Open the USART1 console (`COMxx` @ 115200 via the ST-Link V2 VCP). The console
prints the Dhrystones/s and DMIPS/MHz lines every ~10 s; capture a few seconds
longer than one full run to get a clean result line.

## Notes

* **SysTick**: `board.c` defines `SysTick_Handler` -> `HAL_IncTick()`. Without
  it the SysTick (enabled by `HAL_Init`) jumps into the startup weak handler
  (an infinite `b .` loop) the moment the first tick fires, so the firmware
  hangs with no output.
* **RUN_NUMBER**: kept at 12,000,000 - at 216 MHz a run takes ~10 s,
  comfortably above the 2 s `Too_Small_Time` gate.
* **Do not use LTO for Dhrystone**: GCC `-flto` hoists loop-invariant work out
  of the timed loop and inflates the score (a known GCC artifact).
* Clock config: copied from the vendor 216 MHz template including the 4 GB MPU
  region and OverDrive.
* Console: USART1 (PA9/PA10, AF7) via the on-board ST-Link V2 VCP
  (`COMxx` @ 115200).

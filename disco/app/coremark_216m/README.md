# CoreMark 1.0 @ 216 MHz — STM32F769NI (STM32F769I-Discovery)

EEMBC CoreMark 1.0 (stock `coremark_1_0_1` sources), **25,000 iterations**, on
the STM32F769I-Discovery board (STM32F769NI) clocked at **216 MHz** (HSE
25 MHz, PLL M=25 N=432 P=2 → SYSCLK 216 MHz, HCLK 216 MHz, APB1 54 MHz,
APB2 108 MHz, OverDrive on — clock tree copied verbatim from the vendor 216 MHz
template). Compiler-agnostic: the same sources build with either
**GNU arm-none-eabi-gcc** or **armclang** (AC6 / the LLVM embedded toolchain),
selected at configure time.

## Results (measured on hardware, 216 MHz, hard-float, I/D caches on)

| Toolchain  | Flags                                      | CoreMark 1.0 | Iterations/s | Total time |
| ---------- | ------------------------------------------ | ------------ | ------------ | ---------- |
| GCC 15.3.1 | `-Ofast -ffp-contract=fast -funroll-loops` | 932.52       | 932.52       | 26.81 s    |

Measured on hardware: the build printed **`Correct operation validated.`** with
the expected CRCs (seedcrc 0xe9f5, crcfinal 0xcc42).

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

Open the USART1 console (`COMxx` @ 115200 via the ST-Link V2 VCP). Capture at
least ~11 s so one full (~10 s) run completes and the final
`CoreMark 1.0 : <score> / <compiler> / Static` line is printed.

## Notes

* **SysTick**: `board.c` defines `SysTick_Handler` → `HAL_IncTick()` (same
  requirement as the Dhrystone port — without it the core wedges in the weak
  handler on the first tick).
* **ITERATIONS**: 25,000 — at 216 MHz a run takes ~10 s, valid (CoreMark
  rejects runs shorter than 10 s).
* Port uses `SEED_VOLATILE` (fixed volatile seeds, so the known-CRC validation
  still matches), `MEM_LOCATION "Static"`, `HAS_FLOAT 1`, and the CORE_TICKS
  timer is `HAL_GetTick()` (1 ms SysTick).
* Clock config: copied from the vendor 216 MHz template including the 4 GB MPU
  region and OverDrive.
* Console: USART1 (PA9/PA10, AF7) via the on-board ST-Link V2 VCP
  (`COMxx` @ 115200).

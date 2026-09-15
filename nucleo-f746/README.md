# nucleo-f746 - STM32F746ZG (NUCLEO-F746ZG) board project

Firmware projects for the **NUCLEO-F746ZG** board (STM32F746ZG, Cortex-M7 @
216 MHz).

![NUCLEO-F746ZG board](board_images/board_0.avif)

## Board (hardware)

* MCU: STM32F746ZGTx (LQFP144, 1 MB flash, 320 KB RAM, 216 MHz max, FPU).
* HSE: 8 MHz, supplied by the on-board ST-Link **MCO (bypass mode)** - the
  Nucleo has no HSE crystal; the ST-Link generates the 8 MHz clock.
* LEDs (all on GPIOB, **high active**, `GPIO_PIN_SET` = ON) - same wiring as
  the nucleo-f722 board:
  * **LD1** on **PB0** (green)
  * **LD2** on **PB7** (blue)
  * **LD3** on **PB14** (red)
* Console: **USART3** on **PD8 (TX) / PD9 (RX)**, AF7, 115200 8-N-1 -> the
  on-board ST-Link VCP (`COMxx`).
* Debug probe: on-board ST-Link V2-1 (SWD) - same probe also provides the VCP
  console; no external hardware needed.

## Clock (216 MHz)

```
HSE 8 MHz (ST-Link MCO, bypass) -> PLL (M=8, N=432, P=2) -> SYSCLK 216 MHz
  AHB /1  -> HCLK 216 MHz
  APB1 /4 -> 54 MHz
  APB2 /2 -> 108 MHz
  OverDrive on, VOS scale 1, flash latency 7
```

(Copied verbatim from the vendor `STM32F746ZG-Nucleo` template; identical PLL
setup to the nucleo-f722.)

## Memory plan

* **FLASH (1 MB @ `0x08000000`)** - code + read-only data + `.data`
  initializers.
* **DTCM (64 KB @ `0x20000000`)** - the stack (top-down from `0x20010000`)
  plus `.data`.
* **SRAM1 (112 KB @ `0x20020000`)** - `.bss` plus the `malloc()` heap (grows
  up from `end` toward `0x20040000`; the contiguous SRAM2 16 KB @
  `0x2003C000` is included in that range). SRAM4 (4 KB) and the backup SRAM
  are unused. No external RAM on this board.

Compared to the nucleo-f722: twice the flash (1 MB vs 512 KB) and more SRAM
(320 KB vs 256 KB total; the heap-area SRAM1+SRAM2 is 128 KB vs 192 KB).

## Sharing the F7 HAL / CMSIS

The F7 **HAL + CMSIS drivers are VENDORED in the repo root `drivers/` folder**
(a trimmed STM32Cube_FW_F7 v1.17.4 `Drivers` subtree - see `../drivers/README.md`),
so the repo builds without any external package install. The location is
resolved once in `../cmake/stm32cubef7.cmake`; to use a full STM32Cube_FW_F7
package instead, configure with
`-DSTM32F7_HAL_ROOT=<package>/Drivers`. The board layer (`board/`, `cmake/`)
and the projects live in this board's own folder; the toolchain files
(gcc / Keil armclang / ST starm-clang) are also this board's own, in
`cmake/`.

## FPU: single-precision only (SFPU)

The STM32F746ZG has an **SFPU** — a single-precision FPU only (like the
F722; the F769I is the one with the double-precision FPU). The board layer
therefore builds with **`-mfloat-abi=hard -mfpu=fpv5-sp-d16`**:

* C `float` math uses the single-precision VFP (`vmul.f32`/`vdiv.f32`) - fast.
* C `double` math falls back to **software float** (libgcc) - the F746 has no
  double-precision FPU, so the compiler must not emit double-VFP instructions.

`board.c Board_Init()` also disables the divide-by-zero trap (`CCR.DIV_0_TRP`)
so any internal integer divide-by-zero in newlib's `%f` formatter returns 0
instead of HardFaulting. Together this lets `printf("%f", ...)` work exactly
like on nucleo-f722.

## Projects

| Project        | What it is                                     |
| -------------- | ---------------------------------------------- |
| `bare/blink_hello` | 3-LED blink + USART3 freq print + ADC internal channels |
| `bare/dhry_216m` | Dhrystone 2.1 benchmark @ 216 MHz (best: 2.756 DMIPS/MHz, Keil AC6) |
| `bare/coremark_216m` | CoreMark 1.0 @ 216 MHz (best: 1061.93 it/s, Keil AC6 `-Omax`) |
| `bare/ssd1306_md096_128x64_spi` | SSD1306 0.96" 128x64 mono OLED on SPI1 (integrated from the local `nucleo144-f746zg` repo; patterns per the f425-start I2C port) |

The two benchmarks build with **arm-none-eabi-gcc** (default), **Keil Arm
Compiler 6 (armclang)** and — CoreMark — **ST Arm clang** (starm-clang),
selected with `-DSTM32_TOOLCHAIN=gcc|armclang|starm-clang` at configure time;
optimization flags are the `BENCH_OPT` / C-only `BENCH_OPT_C` cache
variables. Measured results are in each project's `README.md`.

Each project folder has `build.sh`, `CMakeLists.txt`, `src/` and a `README.md`.

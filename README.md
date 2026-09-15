# stm32f7-demo - STM32F7 development projects

Bare-metal firmware projects for multiple **STM32F7** boards (Cortex-M7),
built with CMake + Ninja + GNU arm-none-eabi-gcc (Keil AC6/armclang optional).

Each board lives in its own folder and contains its board layer (`board/`,
`cmake/`) plus the bare-metal applications (`bare/`). The F7 **HAL + CMSIS
drivers are VENDORED in the repo root `drivers/` folder** (a trimmed
STM32Cube_FW_F7 v1.17.4 `Drivers` subtree - see `drivers/README.md`), so the
repo builds without any external package install; the location is resolved in
`cmake/stm32cubef7.cmake` (`-DSTM32F7_HAL_ROOT=<package>/Drivers` overrides
it). Toolchain files (gcc / Keil armclang / ST starm-clang) live in each
board's own `cmake/` folder.

## Boards

| Folder        | Board                     | MCU          | Console                            |
| ------------- | ------------------------- | ------------ | ---------------------------------- |
| `disco-f769/` | STM32F769I-Discovery      | STM32F769NI  | USART1 (PA9/PA10), ST-Link V2 VCOM |
| `nucleo-f722/`| NUCLEO-F722ZE             | STM32F722ZE  | USART3 (PD8/PD9), ST-Link VCP      |
| `nucleo-f746/`| NUCLEO-F746ZG             | STM32F746ZG  | USART3 (PD8/PD9), ST-Link VCP      |

* `disco-f769/` - the STM32F769I-Discovery board (216 MHz): benchmarks
  (`dhry_216m`, `coremark_216m`), SDRAM/QSPI/LCD/Ethernet demos, and external
  QSPI boot (`app_qspi/`, `tool/`). See `disco-f769/README.md`.
* `nucleo-f722/` - the NUCLEO-F722ZE board (216 MHz), gcc/armclang/
  starm-clang benchmark builds. See `nucleo-f722/README.md`.
* `nucleo-f746/` - the NUCLEO-F746ZG board (216 MHz); same LEDs/console
  wiring as the nucleo-f722 (PB0/PB7/PB14 LEDs, USART3 PD8/PD9), with
  `blink_hello`, the same benchmark set, an SSD1306 OLED demo and an
  Ethernet web server (`eth_http_server`, static IP 192.168.5.210).
  See `nucleo-f746/README.md`.

The web app lives in a standalone `e_server/` folder at the repo root (web
frontend + host-side reference C backend). Each Ethernet-capable board
carries its own **copy** under its board folder (`disco-f769/e_server/`,
`nucleo-f746/e_server/`) - the `web/` + `public/` sources there are what the
board's projects pack into `web_assets.h` and serve.

## Build & flash

Each project has its own `build.sh` + `CMakeLists.txt`, then `ninja flash` via
probe-rs on the board's on-board ST-Link:

```bash
cd nucleo-f722/bare/blink_hello
bash build.sh                # == cmake -G Ninja .. && ninja  (GCC, default)
ninja flash                  # probe-rs (SWD) on the on-board ST-Link
```

If several boards are attached, pin the probe at configure time with
`-DDEBUG_PROBE=<selector>` (from `probe-rs list`).

Console per board as listed above at 115200 8-N-1 (`COMxx`, varies per
machine). Board-specific details (hardware, clock tree, memory map, flashing)
are in each board's `README.md`.

## Projects (per board)

* `blink_hello` - 3-LED blink + UART console (current frequency), and the
  on-chip ADC internal channels (die temperature / VREFINT / battery voltage).
* Benchmarks: `dhry_216m` (Dhrystone 2.1) and `coremark_216m` (CoreMark 1.0)
  on all three boards @ 216 MHz (nucleo-f722 and nucleo-f746: up to 2.756
  DMIPS/MHz / 1075.82 and 1061.93 CoreMark respectively with Keil AC6;
  disco-f769: 1.370 DMIPS/MHz / 932.28 CoreMark - the Dhrystone gap is the
  on-chip SRAM heap vs the external SDRAM heap).
* (disco-f769) SDRAM/QSPI/LCD/eth demos, QSPI boot - see
  `disco-f769/README.md`.
* (nucleo-f746) `ssd1306_md096_128x64_spi` OLED demo, `eth_http_server`
  web server (static IP 192.168.5.210, lwIP raw API) - see
  `nucleo-f746/README.md`.

Measured benchmark scores are in each project's `README.md`.

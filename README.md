# stm32f7-demo - STM32F7 development projects

Bare-metal firmware projects for multiple **STM32F7** boards (Cortex-M7),
built with CMake + Ninja + GNU arm-none-eabi-gcc (Keil AC6/armclang optional).

Each board lives in its own folder and contains its board layer (`board/`,
`cmake/`) plus the bare-metal applications (`bare/`). The F7 **HAL + CMSIS
drivers come from a single shared STM32Cube_FW_F7 package** (see
`cmake/stm32cubef7.cmake` for `STM32CUBE_F7`), so no board duplicates the
driver tree.

## Boards

| Folder        | Board                     | MCU          | Console                            |
| ------------- | ------------------------- | ------------ | ---------------------------------- |
| `disco-f769/` | STM32F769I-Discovery      | STM32F769NI  | USART1 (PA9/PA10), ST-Link V2 VCOM |
| `nucleo-f722/`| NUCLEO-F722ZE             | STM32F722ZE  | USART3 (PD8/PD9), ST-Link VCP      |

* `disco-f769/` - the STM32F769I-Discovery board (216 MHz): benchmarks
  (`dhry_216m`, `coremark_216m`), SDRAM/QSPI/LCD/Ethernet demos, and external
  QSPI boot (`app_qspi/`, `tool/`). It carries its own vendored F7 HAL/CMSIS
  (from the same STM32Cube_FW_F7 package); the other boards share the package
  directly. See `disco-f769/README.md`.
* `nucleo-f722/` - the NUCLEO-F722ZE board (216 MHz), shares the F7
  HAL/CMSIS from the package. See `nucleo-f722/README.md`.

There is also a standalone `e_server/` (repo root) with the web app + reference
backend used by the `disco-f769/bare/eth_http` demo.

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
* (disco-f769) benchmarks, SDRAM/QSPI/LCD eth demos, QSPI boot - see
  `disco-f769/README.md`.

Measured benchmark scores are in each project's `README.md`.

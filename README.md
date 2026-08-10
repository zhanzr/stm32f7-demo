# stm32f769_prj — STM32F769 development projects

Bare-metal firmware projects for the **STM32F769I-Discovery** board
(STM32F769NI, Cortex-M7 @ 216 MHz), built with CMake + Ninja +
GNU arm-none-eabi-gcc (Keil AC6/armclang optional).

The whole board project lives in the **`disco/`** folder — it holds the shared
board layer (`board/`, `cmake/`, `drivers/`) plus the applications under
`disco/app/`. Each application has its own `build.sh`, `CMakeLists.txt`,
`src/` and a `README.md` with the project-specific build/flash/measure
instructions and its measured benchmark results.

## Build & flash

```bash
cd disco/app/<project>
bash build.sh                # == cmake -G Ninja .. && ninja  (GCC, default)
ninja flash                  # probe-rs --chip STM32F769NI via on-board ST-Link V2
```

Console: USART1 at 115200 8-N-1 on the ST-Link V2 VCOM (`COMxx`, number varies
per machine). Board-specific details (hardware, clock tree, images, flashing)
are in `disco/README.md`.

## Projects

* `blink_hello` — 3-LED blink + UART console, also measures the on-chip ADC
  internal channels (die temperature / VREFINT / battery voltage).
* `dhry_216m` — Dhrystone 2.1 benchmark @ 216 MHz.
* `coremark_216m` — CoreMark 1.0 benchmark @ 216 MHz.
* `qspi_flash_test` — MX25L51245G QSPI flash erase/program/read benchmark.
* `sdram_test` — on-board SDRAM write/read/memcpy benchmark.
* `lcd_touch_test` — MIPI DSI LCD (OTM8009A 800x480) + FT6206 touch demo.

Measured benchmark scores are in each project's `README.md`.

# nucleo-f722 - STM32F722ZE (NUCLEO-F722ZE) board project

Firmware projects for the **NUCLEO-F722ZE** board (STM32F722ZE, Cortex-M7 @
216 MHz).

## Board (hardware)

* MCU: STM32F722ZETx (LQFP144, 512 KB flash, 256 KB RAM, 216 MHz max, FPU).
* HSE: 8 MHz, supplied by the on-board ST-Link **MCO (bypass mode)** - the
  Nucleo has no HSE crystal; the ST-Link generates the 8 MHz clock.
* LEDs (all on GPIOB, **high active**, `GPIO_PIN_SET` = ON):
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

(Copied verbatim from the vendor `STM32F722ZE-Nucleo` template.)

## Memory plan

* **FLASH (512 KB @ `0x08000000`)** - code + read-only data + `.data`
  initializers.
* **DTCM (64 KB @ `0x20000000`)** - the stack (top-down from `0x20010000`)
  plus `.data`.
* **SRAM1 (176 KB @ `0x20010000`)** - `.bss` plus the `malloc()` heap (grows up
  from `end` toward `0x20040000`). No external RAM on this board.

## Sharing the F7 HAL / CMSIS

The F7 **HAL + CMSIS drivers come from the single shared STM32Cube_FW_F7
package** installed on this machine (see `../cmake/stm32cubef7.cmake` for the
`STM32CUBE_F7` path - override with `-DSTM32CUBE_F7=`). Every board in this
repo uses that one package; the board layer (`board/`, `cmake/`) and the
projects live in each board's own folder. Override the core frequency with
`BOARD_PLL_N` if needed.

## Projects

| Project        | What it is                                     |
| -------------- | ---------------------------------------------- |
| `bare/blink_hello` | 3-LED blink + USART3 freq print + ADC internal channels |

Each project folder has `build.sh`, `CMakeLists.txt`, `src/` and a `README.md`.

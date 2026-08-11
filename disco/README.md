# disco 鈥?STM32F769I-Discovery board project

Firmware projects for the **STM32F769I-Discovery** board (STM32F769NI @
216 MHz).

![Board photos](board_0.png)

![board_1](board_1.png)
![board_2](board_2.png)
![board_3](board_3.png)
![board_4](board_4.png)

## Board (hardware)

* MCU: STM32F769NI (BGA216, 2 MB flash, 512 KB SRAM, 216 MHz max, dual FPU).
* HSE: 25 MHz external crystal.
* LEDs (all **high active**, `GPIO_PIN_SET` = ON):
  * **LD1** on **PJ13** (green)
  * **LD2** on **PJ5** (red)
  * **LD3** on **PA12** (green)
* USART1 console: PA9 (TX) / PA10 (RX), AF7, 115200 8-N-1 鈫?the on-board
  **ST-Link V2 VCOM** (`COMxx`).
* Debug probe: on-board **ST-Link V2 (SWD)** 鈥?same probe also provides the VCP
  console, no external hardware needed.

## Clock configuration (216 MHz)

```
HSE 25 MHz 鈫?PLL (M=25, N=432, P=2) 鈫?SYSCLK 216 MHz
  AHB /1  鈫?HCLK 216 MHz
  APB1 /4 鈫?54 MHz
  APB2 /2 鈫?108 MHz
  OverDrive on, VOS scale 1, flash latency 7
```

Copied verbatim from the vendor 216 MHz template (the STM32Cube_FW_F7 package).

## SDRAM (built-in RAM pool)

The on-board **16 MB SDRAM** (FMC bank 1 @ `0xC0000000`) is brought up by
`Board_Init()` 鈫?`SDRAM_Init()` 鈫?`BSP_SDRAM_Init()` (vendor BSP) at the
216 MHz clock. It is divided into **two linker-managed regions** (no raw
addresses are needed anywhere):

* **`.sdram`** 鈥?opt-in zero-init buffers (`_sdram_start`..`_sdram_end`,
  cleared by `SDRAM_Init`). Use it for large buffers:
  ```c
  uint8_t big_buf[4096] __attribute__((section(".sdram")));
  ```
* **`._user_heap`** 鈥?the `malloc()` heap, placed *after* `.sdram` and capped at
  the physical SDRAM top (`0xC0000000` + 16 MB = `0xC1000000`) by `_sbrk` in `syscalls.c`. All `malloc()` calls
  after `Board_Init()` land in the SDRAM.

The MPU opens the FMC controller (`0xA0000000`) and the SDRAM (`0xC0000000`)
as **write-through cacheable** (coherent with other bus masters like the
LTDC/DMA).

Design notes:

* **Stack + `.data`/`.bss` stay in internal SRAM (DTCM).** The Cortex-M7 loads
  the initial SP from the vector table *before* `SystemInit()` runs, and the
  startup `.data` copy must not touch the external SDRAM: on this board, SDRAM
  writes fault (imprecise bus error) during the pre-main bootstrap.
* **The heap needs a tiny DTCM reserve.** newlib calls `_sbrk()`/`malloc()` a
  few times during its own C-runtime init (`__libc_init_array`, before `main()`)
  鈥?at a point where SDRAM accesses still fault. `_sbrk` therefore serves those
  pre-main allocations from a small `.dcm_heap` reserve (8 KB in DTCM) and only
  switches to the SDRAM heap after `Board_Init()` has brought the SDRAM up
  (`Board_SdramReady()`).
* Because the malloc'd working set lives in the SDRAM, benchmarks that hammer
  malloc'd records slow down accordingly (e.g. Dhrystone ~1.37 vs ~2.72
  DMIPS/MHz with a DTCM heap); CoreMark is essentially unaffected.

## Memory usage (measured, GCC 15.3.1, 216 MHz)

Per project, from the linker maps:

| Project | FLASH (2 MB) | DTCM (128 KB) | SDRAM (16 MB): heap + `.sdram` |
| ------- | ------------ | ------------- | ------------------------------ |
| `blink_hello` | 40.6 KB | 10.9 KB | 16 MB heap |
| `dhry_216m` | 44.2 KB | 20.9 KB | 16 MB heap |
| `coremark_216m` | 61.4 KB | 12.8 KB | 16 MB heap |
| `qspi_flash_test` | 43.4 KB | 74.9 KB | 16 MB heap |
| `sdram_test` | 39.5 KB | 10.8 KB | 8 MB `.sdram` + 8 MB heap |
| `lcd_touch_test` | 71.6 KB | 11.6 KB | 1.5 MB `.sdram` (framebuffer) + ~14.5 MB heap |
| `disco_boot` | 41.6 KB | 10.7 KB | 16 MB heap |
| `app_qspi/*` | ~40–72 KB in QSPI flash (8 MB @ 0x90000000) | ~11–21 KB | 16 MB heap (`.sdram` where used) |

Notes:

* **FLASH** = code + read-only data + `.data` initializers (internal flash,
  `0x08000000`; QSPI apps link at `0x90000000` instead).
* **DTCM** = `.data` + `.bss` + the 8 KB `.dcm_heap` reserve. The stack takes
  the rest of the 128 KB (top-down from `0x20020000`).
* **SDRAM heap** is the space available to `malloc()` (from `end` up to
  `0xC1000000`); `.sdram` is the opt-in fixed buffer pool. `sdram_test` holds
  an 8 MB `.sdram` array, the LCD holds its 800x480x4 framebuffer.

## Projects

| Project          | What it is                                     |
| ---------------- | ---------------------------------------------- |
| `blink_hello`    | 3-LED blink + UART freq print + ADC internal channels |
| `dhry_216m`      | Dhrystone 2.1 benchmark @ 216 MHz              |
| `coremark_216m`  | CoreMark 1.0 @ 216 MHz                         |
| `qspi_flash_test`| MX25L51245G QSPI flash erase/program/read benchmark |
| `sdram_test`     | on-board SDRAM write/read/memcpy benchmark     |
| `lcd_touch_test` | MIPI DSI LCD (OTM8009A) + FT6206 touch demo    |

Each project folder contains its own README with build/flash instructions and
the measured benchmark results.

### External QSPI boot (`app_qspi/` + `tool/`)

The board can also boot firmware from the MX25L51245G at `0x90000000`:

| Path | What it is |
| ---- | ---------- |
| `tool/disco_boot` | bootloader in internal flash: inits QUADSPI, checks `0x90000000`, jumps |
| `app_qspi/blink_hello_qspi` | same app as `blink_hello` (LEDs + ADC) booted from QSPI |
| `app_qspi/dhry_216m_qspi` | Dhrystone 2.1 benchmark from QSPI (1.44 DMIPS/MHz, SDRAM heap) |
| `app_qspi/coremark_216m_qspi` | CoreMark 1.0 from QSPI (930.93, validated) |
| `app_qspi/lcd_touch_test_qspi` | MIPI DSI LCD demo + touch from QSPI |
| `tool/qspi_map/algo` | probe-rs QUADSPI flash algorithm for the MX25L51245G |
| `tool/probers_alg` | bring-up harness that runs the algorithm's register code with a UART trace |

One-time setup per board: flash `tool/disco_boot` to internal flash, then any
`app_qspi/*` project's `ninja flash` programs the MX25L51245G via the algorithm
and resets the board to boot the app. Verified on hardware (boot chain in
`tool/disco_boot/README.md`).

> `qspi_flash_test` and `sdram_test` are **not** ported to QSPI: a flash
> benchmark would erase/program the very flash the app executes from (breaking
> the memory-mapped mapping), and `sdram_test` is kept only in internal flash
> for later use.

## Build & flash

Each project has `build.sh` (GNU arm-none-eabi-gcc, the default) and supports
a separate `build-ac6/` dir for Keil AC6 (armclang). The build outputs the
`.elf` + `.hex`; `ninja flash` programs the board via probe-rs + the on-board
ST-Link V2, and `ninja dfu-flash` via USB DFU (fallback):

```bash
cd bare/blink_hello
bash build.sh
ninja flash        # probe-rs download --chip STM32F769NI via ST-Link V2 (SWD)
ninja dfu-flash    # USB DFU via STM32CubeProgrammer (needs BOOT0=1 + reset)
```

Then open the USART1 console at 115200 8-N-1 (the ST-Link V2's virtual COM
port 鈥?`COMxx`, the number varies per machine).

> `ninja flash` auto-detects the connected probe (a single ST-Link V2). To pin
> a specific probe, view its selector with `probe-rs list` (e.g.
> `0483:374b:xxxx...` for an ST-Link V2 鈥?every unit has its own serial) and
> pass `-DDEBUG_PROBE=<selector>` at configure time.

## Reference material

* The `main-stm32f769i-disco` folder of the local board database: user manual
  (UM2033), schematics (MB1166 / MB1225), programming manual (PM0253) and
  reference manual (RM0410).
* The `STM32F769I-Discovery` BSP under the STM32Cube_FW_F7 package
  (`Drivers/BSP/STM32F769I-Discovery`).

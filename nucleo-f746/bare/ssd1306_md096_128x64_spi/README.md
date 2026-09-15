# ssd1306_md096_128x64_spi - SSD1306 0.96" 128x64 mono OLED on SPI1 (NUCLEO-F746ZG)

Drives a **0.96" 128x64 mono OLED** (MD096 module, SSD1306 controller) over
**SPI1**, integrating the `ssd1306_oled096_spi` branch of the local
`D:/nucleo144-f746zg` repository (hardware connection + SSD1306 command
flow). The display pattern set follows the `f425-start` repo's
`bare/ssd1306_md096_128x64_iic` demo.

## Hardware connection (from the local repo's `ssd1306_oled096_spi` branch)

| OLED pin | Nucleo pin | Note                          |
| -------- | ---------- | ----------------------------- |
| VCC      | 3V3        |                               |
| GND      | GND        |                               |
| SCK      | **PA5**    | SPI1_SCK, AF5                 |
| MISO     | **PA6**    | SPI1_MISO, AF5 (unused by the TX-only driver) |
| MOSI     | **PA7**    | SPI1_MOSI, AF5                |
| CS       | **PD14**   | soft chip select              |
| DC       | **PD15**   | 0 = command, 1 = data         |
| RES      | **PF12**   | hardware reset pulse at init  |
| BL       | **PF13**   | driven high (no backlight on mono OLED; kept for wiring compatibility) |

SPI1: master, mode 0 (CPOL 0, first edge), 8-bit MSB first, soft NSS,
prescaler 32 -> APB2 108 MHz / 32 = **3.375 MHz** - all as in the source
repo's `Core/Src/spi.c`.

## Differences from the source branch's driver

* **`Set_Pos` fix**: the branch driver's low-nibble command ORed `0x01`
  (`(x&0x0f)|0x01`), shifting every column by one and leaving the last
  column unused - fixed to plain `x & 0x0f` (same fix as the f425-start
  port).
* **Batched page writes**: one full 128-byte page row goes out in a single
  SPI transmit with DC held high - 8 transfers per full screen instead of
  1024 single-byte ones. Measured **317 fps** for the animated 16-px bar
  sweep (40 frames, `oled fps:` on the console).

## Demo (patterns per the f425-start `ssd1306_md096_128x64_iic` port)

```
banner ("SSD1306 MD096 / 128x64 mono / STM32F746ZG / 216MHz SPI1")
  -> same page whole-display inverted
  -> all pixels on -> off
  -> 8-px horizontal stripes -> 4-px horizontal stripes
  -> 1-px checkerboard -> 16-px vertical bars
  -> 1-px border rectangle -> page-gradient "grey" ramp
  -> animated 16-px bar sweep + console FPS report
```

The three on-board LEDs (PB0/PB7/PB14) toggle between demo passes; the
console (USART3, PD8/PD9, ST-Link VCP @ 115200) prints the banner, the demo
pass lines and the fps:

```
=== ssd1306_md096_128x64_spi on STM32F746ZG @ 216000000 Hz ===
oled fps: fps 317
demo pass done @ 11886 ms
```

## Build & flash

```bash
bash build.sh          # == cmake -G Ninja .. && ninja  (GNU arm-none-eabi-gcc)
ninja flash            # probe-rs (SWD) on the on-board ST-Link V2-1
```

If several ST-Links are attached, pin the nucleo probe at configure time:
`cmake -G Ninja -DDEBUG_PROBE=0483:3752:xxxx... ..`.

## Files

* `src/oled.c` / `src/oled.h` - SSD1306 driver (transport + text + patterns).
  Transport per the local repo branch; API/patterns per the f425-start port.
* `src/oledfont.c` - F6x8 / F8X16 ASCII bitmaps (from the f425-start port).
* `src/main.c` - board bring-up + demo loop.

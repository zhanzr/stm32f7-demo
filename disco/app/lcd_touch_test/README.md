# lcd_touch_test — MIPI DSI LCD dynamic demo + capacitive touch @ 216 MHz

Drives the **4" 800x480 MIPI DSI** LCD (OTM8009A panel) in **DSI video mode**
through LTDC with the SDRAM as framebuffer, plus the **FT6206** capacitive
touch controller on I2C4 — all via the vendor BSP
(`stm32f769i_discovery_lcd` / `stm32f769i_discovery_ts`).

The screen shows a looping dynamic demo (adapted from the h723-mini ST7789
demo) while the touch sensor stays active the whole time: a marker follows the
finger, and the coordinates are printed on the USART1 console.

## Display (MIPI DSI)

* Interface: **MIPI DSI** host in **video (burst) mode**, 2 data lanes.
* Panel: **OTM8009A**, 800x480 (landscape), RGB888, DSI video mode.
* Framebuffer: on-board 16 MB SDRAM at `0xC0000000` (write-through cacheable
  so the LTDC, a separate bus master, sees CPU writes).
* Graphics: LTDC + DMA2D + the vendor fonts.

## Demo phases (loop forever)

| Phase | What you see |
| ----- | ------------ |
| **Shapes** | 10 floating/bouncing squares, circles, triangles (5 s) |
| **Pure colors** | RED…BLACK full-screen, one per color (4 s each) |
| **Gradient** | animated hue sweep down the color wheel (5 s) |
| **LED test** | the three on-board LEDs LD1/LD2/LD3 toggle on/off (3 s each) |

A status band at the bottom always shows **`FPS:<n> | T:<x,y>`** (touch
coordinates, `--` when no finger is down).

## Touch (FT6206)

* Controller: **FT6206** capacitive touch IC on **I2C4** (PD12/PB7).
* Active during every demo phase: a **white-ring/red-dot marker** follows the
  finger, and the console prints `Touch: X=... Y=...` on movement and
  `Touch: released` on lift.
* `BSP_TS_Init()` auto-applies the landscape coordinate swap; reported
  coordinates are already in 800x480 LCD space. Polled `BSP_TS_GetState()`
  (no EXTI used) once per frame.

## Build

```bash
bash build.sh                      # == cmake -G Ninja .. && ninja
```

`ninja` builds the `.elf` + `.hex` (the `.hex` is what `ninja flash` programs).

## Flash & observe

```bash
ninja flash        # probe-rs through the on-board ST-Link V2 (SWD)
```

The LCD should cycle through the dynamic phases; touch the glass to leave a
marker and watch the coordinates on the USART1 console (`COMxx` @ 115200).

## Notes

* Uses the vendor BSP + components (`otm8009a`, `nt35510`, `ft6x06`) and fonts
  from the `vendor/` tree under the board root.
* The LTDC/DMA2D/DSI NVIC lines are enabled by the BSP; this project provides
  their handlers (startup defaults are an infinite loop).
* D-cache stays enabled; the SDRAM framebuffer region is write-through so no
  cache maintenance is needed for LTDC visibility.
* Touch markers are clamped to the screen so the unclipped BSP fill helpers
  never write outside the framebuffer (which would hit an MPU-denied region).

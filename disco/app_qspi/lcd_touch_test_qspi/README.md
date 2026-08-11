# lcd_touch_test_qspi 鈥?MIPI DSI LCD demo + touch from the MX25L51245G

Same as `bare/lcd_touch_test` (MIPI DSI LCD dynamic demo + FT6206 touch) but
linked for and booted from the on-board **MX25L51245G** at the QUADSPI
memory-mapped base `0x90000000` (requires `disco_boot` in internal flash). The
app code runs from QSPI while the LTDC/DSI drive the SDRAM framebuffer.

Verified on hardware:

```
QSPI firmware check: PASS - booting ...
=== lcd_touch_test (QSPI) on STM32F769NI @ 216000000 Hz ===
LCD: 800 x 480, MIPI DSI video mode (OTM8009A)
TS: FT6206 OK
[LCD] phase: shapes / pure colors / gradient / LED test
```

## Build & flash

```bash
bash build.sh                 # -> build/lcd_touch_test.hex (linked at 0x90000000)
ninja flash                   # writes the MX25L51245G via the QUADSPI algorithm
```

`disco_boot` must be in internal flash (see `tool/disco_boot/README.md`).

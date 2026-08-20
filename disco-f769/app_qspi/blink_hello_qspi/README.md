# blink_hello_qspi - STM32F769NI (STM32F769I-Discovery) from the MX25L51245G @ 0x90000000

Same as `bare/blink_hello` (LD1/LD2/LD3 high-active LEDs + the ADC internal
channels: die temp / VREFINT / VBAT), but linked for and booted from the
on-board **MX25L51245G** (64 MB) at the QUADSPI memory-mapped base
`0x90000000`. Requires `disco_boot` in internal flash.

The LEDs toggle every second and the console prints the **current frequency and
the ADC values** (no LED ON/OFF line):

```
ADC: VREFINT raw=1502 cal=1503 -> VDDA=3302 mV | Temp=44.2 C | VBAT=3.32 V @ 216000000 Hz
```

## Build & flash

```bash
bash build.sh                 # -> build/blink_hello.hex (linked at 0x90000000)
ninja flash                   # writes the MX25L51245G via the QUADSPI algorithm
```

`ninja flash` regenerates the flash algorithm if needed (`build_algo.py`), then
programs the MX25L51245G via probe-rs (connect-under-reset) and resets the
board, so `disco_boot` boots the new app immediately.

## Notes

* The app uses a **non-destructive** `system_app.c` (`SystemInit` leaves RCC and
  the QUADSPI clock alone - the bootloader owns them).
* `board.c`'s `SystemClock_Config()`/MPU become no-ops under `QSPI_APP` so the
  same `main()` calls `Board_Init()` without breaking the bootloader's clocks.
* Verified on hardware: boots from the MX25L51245G at 216 MHz with correct ADC
  readings (matching the internal-flash run).

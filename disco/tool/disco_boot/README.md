# disco_boot — STM32F769I-Discovery bootloader (internal flash)

Minimal bootloader in internal flash. Brings up the 216 MHz clock tree and
console, initializes the on-board **MX25L51245G** (64 MB) via the QUADSPI, then
performs a basic bootability check on a stage-2 firmware at the QSPI
memory-mapped base `0x90000000`:

* the initial stack pointer (word 0) must land in SRAM, and
* the reset vector (word 1) must be a thumb pointer inside `0x90000000..0x90800000`.

If the check passes it enables QUADSPI memory-mapped mode and jumps to the app.
If it fails it loops, toggling the three LEDs (LD1/LD2/LD3) and printing the
check result every 3 s.

## Build & flash

```bash
bash build.sh       # -> build/disco_boot.hex
ninja flash         # probe-rs -> internal flash (STM32F769NI)
```

## Boot chain (verified on hardware)

```
=== disco_boot bootloader @ 216000000 Hz ===
QSPI init rc=0 (MX25L51245G)
QSPI firmware check: PASS - booting (SP=0x20020000, Reset=0x9000085d)
jumping to 0x90000000 ...
=== blink_hello (QSPI) on STM32F769NI @ 216000000 Hz ===
LED LD1/2/3: ON/OFF @ 216000000 Hz
```

The stage-2 firmware is written to the MX25L51245G separately via the probe-rs
QUADSPI flash algorithm (see `app_qspi/blink_hello_qspi` / `ninja flash` and
`tool/qspi_map/algo/README.md`).

## Notes

* The boot uses the vendor QUADSPI BSP (`stm32f769i_discovery_qspi`), which
  leaves the flash in QPI mode with 4-byte addressing for the memory-mapped
  read; the flash algorithm resets it back to SPI mode on the next flash.
* The boot's MPU adds a cacheable/executable region at `0x90000000` (the app
  runs from there) and a device region for the QUADSPI at `0xA0000000`.

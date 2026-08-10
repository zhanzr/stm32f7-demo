# QUADSPI MX25L51245G flash algorithm (`algo/`)

Position-independent flash algorithm for programming the on-board
**MX25L51245G** (64 MB) via probe-rs, using the STM32F769NI **QUADSPI** in
1-line SPI mode (no QE needed).

## Files

- `flash_mx25l512_qspi.c` — the algorithm (register-level QUADSPI, no HAL).
- `algo.ld` — links it at `0x20000000` for the position-independent blob.
- `build_algo.py` — compiles it and generates `target_mx25l512_qspi.yaml`
  (`python build_algo.py flash_mx25l512_qspi.c 0x1000`).
- `target_mx25l512_qspi.yaml` — probe-rs chip description (auto-generated).

## How it works

* QUADSPI @ `0xA0001000`, clock enabled on `RCC_AHB3ENR` **bit 1**.
* Pins: PB2 CLK AF9, PC9 IO0 AF9, PC10 IO1 AF9, PE2 IO2 AF9, PD13 IO3 AF9.
  **NCS (PB6) is driven manually** as a GPIO output around each transfer (the
  QUADSPI NCS is hi-Z when idle; manual CS is robust regardless of the board's
  CS pull).
* The DR is accessed **byte-granular** (matching the F7 `HAL_QSPI_*`, verified
  on this board via the vendor BSP); an indirect read is started by the second
  AR write.
* `Init` resets the flash out of any QPI/continuous-read mode the previous app
  left it in: a **4-line** 0x66/0x99 (needs all four IO pins) followed by a
  1-line 0x66/0x99, then checks the JEDEC ID `0xC2201A` (MX25L51245G).
* Self-verifying: every page program and sector erase is read back and retried.

## Status (verified on hardware)

| Function     | Status |
|--------------|--------|
| `Init` (JEDEC 0xC2201A) | **OK** |
| `EraseSector` | **OK** |
| `ProgramPage` | **OK** |
| `Verify` | **OK** |
| Full `ninja flash` → boot | **OK** (`disco_boot` boots `blink_hello_qspi`) |

## Bring-up notes (things that were wrong first)

1. **QSPI clock bit**: the QUADSPI clock enable is `RCC_AHB3ENR` bit 1 (not a
   bit in the middle of the register) — the algorithm's Init timed out until
   this was fixed.
2. **QPI-mode flash**: the boot/app leave the MX25L512 in QPI mode, so 1-line
   SPI commands are ignored. The reset must be sent **4-line** first, which
   needs **all four IO pins** (PE2/PD13 too) configured.
3. **JEDEC ID**: the MX25L51245G reports `0xC2201A` (not `0xC22538`).
4. Debugged with `../probers_alg` (a firmware harness that runs the same
   register code with a UART trace).

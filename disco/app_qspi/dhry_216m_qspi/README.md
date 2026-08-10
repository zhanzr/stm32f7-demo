# dhry_216m_qspi — Dhrystone 2.1 @ 216 MHz from the MX25L51245G

Same as `app/dhry_216m` but linked for and booted from the on-board
**MX25L51245G** at the QUADSPI memory-mapped base `0x90000000` (requires
`disco_boot` in internal flash).

## Results (measured on hardware, 216 MHz, GCC 15.3.1)

| Location | Dhrystones/s | DMIPS/MHz |
| -------- | ------------ | --------- |
| internal flash (`app/dhry_216m`) | 1,033,325 | 2.723 |
| **MX25L51245G (`app_qspi/dhry_216m_qspi`)** | **1,033,325** | **2.723** |

Identical to the internal-flash run — Dhrystone's code is small and
cache-resident, so the QSPI fetch path doesn't slow it down.

## Build & flash

```bash
bash build.sh                 # -> build/dhry_216m.hex (linked at 0x90000000)
ninja flash                   # writes the MX25L51245G via the QUADSPI algorithm
```

`disco_boot` must be in internal flash (see `tool/disco_boot/README.md`).

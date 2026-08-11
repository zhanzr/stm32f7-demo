# dhry_216m_qspi 鈥?Dhrystone 2.1 @ 216 MHz from the MX25L51245G

Same as `bare/dhry_216m` but linked for and booted from the on-board
**MX25L51245G** at the QUADSPI memory-mapped base `0x90000000` (requires
`disco_boot` in internal flash).

## Results (measured on hardware, 216 MHz, GCC 15.3.1)

| Location | Dhrystones/s | DMIPS/MHz |
| -------- | ------------ | --------- |
| internal flash (`bare/dhry_216m`) | 519,998 | 1.370 |
| **MX25L51245G (`app_qspi/dhry_216m_qspi`)** | **547,021** | **1.441** |

The malloc'd Dhrystone records live in the on-board **SDRAM** (two-region
heap, write-through cacheable), which is why the scores are ~half a DTCM-heap
build; the QSPI code-fetch path itself adds little (identical build as
`bare/dhry_216m`).

## Build & flash

```bash
bash build.sh                 # -> build/dhry_216m.hex (linked at 0x90000000)
ninja flash                   # writes the MX25L51245G via the QUADSPI algorithm
```

`disco_boot` must be in internal flash (see `tool/disco_boot/README.md`).

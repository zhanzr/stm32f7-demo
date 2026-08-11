# coremark_216m_qspi 鈥?CoreMark 1.0 @ 216 MHz from the MX25L51245G

Same as `bare/coremark_216m` but linked for and booted from the on-board
**MX25L51245G** at the QUADSPI memory-mapped base `0x90000000` (requires
`disco_boot` in internal flash).

## Results (measured on hardware, 216 MHz, GCC 15.3.1)

| Location | CoreMark 1.0 | Total time |
| -------- | ------------ | ---------- |
| internal flash (`bare/coremark_216m`) | 932.28 | 26.82 s |
| **MX25L51245G (`app_qspi/coremark_216m_qspi`)** | **930.93** | **26.86 s** |

CoreMark's malloc'd list lives in the on-board SDRAM (two-region heap), but it
is a small part of the workload, so the score is close to a DTCM-heap build.
The run is valid: `Correct operation validated.` (seedcrc 0xe9f5, crcfinal
0xcc42).

## Build & flash

```bash
bash build.sh                 # -> build/coremark_216m.hex (linked at 0x90000000)
ninja flash                   # writes the MX25L51245G via the QUADSPI algorithm
```

`disco_boot` must be in internal flash (see `tool/disco_boot/README.md`).

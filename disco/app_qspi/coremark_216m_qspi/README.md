# coremark_216m_qspi — CoreMark 1.0 @ 216 MHz from the MX25L51245G

Same as `app/coremark_216m` but linked for and booted from the on-board
**MX25L51245G** at the QUADSPI memory-mapped base `0x90000000` (requires
`disco_boot` in internal flash).

## Results (measured on hardware, 216 MHz, GCC 15.3.1)

| Location | CoreMark 1.0 | Total time |
| -------- | ------------ | ---------- |
| internal flash (`app/coremark_216m`) | 932.52 | 26.81 s |
| **MX25L51245G (`app_qspi/coremark_216m_qspi`)** | **909.16** | **27.50 s** |

Slightly lower than internal flash — CoreMark is a larger binary, so the QSPI
memory-mapped fetch path (108 MHz) limits code throughput a little. The run is
still valid: `Correct operation validated.` (seedcrc 0xe9f5, crcfinal 0xcc42).

## Build & flash

```bash
bash build.sh                 # -> build/coremark_216m.hex (linked at 0x90000000)
ninja flash                   # writes the MX25L51245G via the QUADSPI algorithm
```

`disco_boot` must be in internal flash (see `tool/disco_boot/README.md`).

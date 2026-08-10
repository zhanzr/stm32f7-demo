# qspi_flash_test — MX25L51245G QSPI flash benchmark @ 216 MHz

Benchmarks the on-board **MX25L51245G** (64 MB) QSPI NOR flash on the
STM32F769I-Discovery via the vendor BSP driver (`stm32f769i_discovery_qspi`)
in **4-wire QPI mode** (QUADSPI at 108 MHz = SYSCLK/2). Indirect (FIFO)
transfers only — **no memory-mapped remap**.

Measured over a 128 KB region (32 × 4 KB subsectors, pages of 256 B):
erase (subsector), program (page), read (indirect) + verify.

## Results (measured on hardware, 216 MHz, GCC 15.3.1, QPI mode)

| Operation | Region | Throughput |
| --------- | ------ | ---------- |
| Subsector erase (4 KB) | 128 KB | **141 KB/s** (28.2 ms/subsector) |
| Page program (256 B) | 128 KB | **876 KB/s** |
| Indirect read | 1 MB | **6.62 MB/s** |

Data verified after every pass (`verify OK`). Timing via the 1 ms SysTick
(`HAL_GetTick`).

## Build

```bash
bash build.sh                      # == cmake -G Ninja .. && ninja
```

`ninja` builds the `.elf` + `.hex` (the `.hex` is what `ninja flash` programs).

## Flash & observe

```bash
ninja flash        # probe-rs through the on-board ST-Link V2 (SWD)
```

Open the USART1 console (`COMxx` @ 115200 via the ST-Link V2 VCP). One
benchmark pass (erase + program + read/verify) runs every few seconds and
prints the throughput lines.

## Notes

* Erase/program are destructive to the test region (`0x000000`). The region is
  reprogrammed with a deterministic pattern each pass, so a reset leaves it
  filled, not erased.
* Timing uses the 1 ms SysTick (`HAL_GetTick`).
* The shared board MPU denies `0xA0000000` (which covers the QUADSPI block);
  this project adds an 8 KB device-memory MPU region there (same as the vendor
  template's FMC control-register region) so the QUADSPI registers are
  accessible.
* The BSP leaves the memory in QPI mode with 4-byte addresses after init (see
  `BSP_QSPI_Init`); only `BSP_QSPI_Read/Write/Erase_Block` are used here.

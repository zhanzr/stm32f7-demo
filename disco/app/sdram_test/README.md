# sdram_test — on-board SDRAM write/read benchmark @ 216 MHz

Benchmarks the on-board **16 MB SDRAM** (IS42S32800J / MT48LC4M32B2-style,
32-bit, FMC SDRAM bank 1 @ `0xC0000000`) on the STM32F769I-Discovery via the
vendor BSP driver (`stm32f769i_discovery_sdram`). No memory remap — the SDRAM
stays at its FMC address.

SDRAM is volatile RAM, so there is no erase/program: this benchmark measures
**write**, **read** and **memcpy** throughput instead.

## Methodology

* 8 MB test region at `0xC0000000`. SDRAM is mapped **non-cacheable** (via an
  MPU region added on top of the shared board layer), so the numbers reflect
  real FMC/SDRAM bus traffic, not L1 cache hits.
* Write: 32-bit store pattern (`0xA5A5A5A5 ^ i`) over the whole region, with a
  `__DSB()` drain included in the timing.
* Read: 32-bit loads summed (minimal per-word overhead), D-cache invalidated
  first; a separate untimed pass verifies data integrity.
* memcpy: SDRAM → SDRAM (half region).
* Timing via the 1 ms SysTick (`HAL_GetTick`).

## Results (measured on hardware, 216 MHz, GCC 15.3.1)

| Operation | Size | Throughput |
| --------- | ---- | ---------- |
| Write (32-bit stores, uncached) | 8 MB | **275.9 MB/s** |
| Read (32-bit loads, uncached) | 8 MB | **70.2 MB/s** |
| memcpy (SDRAM→SDRAM) | 4 MB | **31.5 MB/s** |

Data integrity verified after every pass (`Verify: OK`).

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
write + read + memcpy pass runs every few seconds and prints the throughput
lines.

## Notes

* The test region overwrites the first 8 MB of SDRAM (also the LCD framebuffer
  area) — that's fine here as this project owns the whole chip.
* If the LCD demo was run before, the SDRAM MPU region added here (write-through,
  higher priority than the shared board deny-region) makes the SDRAM accessible.

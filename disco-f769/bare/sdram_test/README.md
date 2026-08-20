# sdram_test - on-board SDRAM write/read benchmark @ 216 MHz

Benchmarks the on-board **16 MB SDRAM** (IS42S32800J / MT48LC4M32B2-style,
32-bit, FMC SDRAM bank 1 @ `0xC0000000`) on the STM32F769I-Discovery via the
vendor BSP driver (`stm32f769i_discovery_sdram`).

SDRAM is volatile RAM, so there is no erase/program: this benchmark measures
**write**, **read** and **memcpy** throughput instead.

## Methodology

* The test buffer is an **8 MB `uint32_t` array in the `.sdram` linker section**
  (the shared board layer's opt-in big-buffer pool, zeroed at init). It is the
  app's *own* memory, so the destructive patterns only ever touch that buffer - they never clobber other app data or the LCD framebuffer.
* The shared board layer brings the SDRAM up (`Board_Init()` -> `BSP_SDRAM_Init`)
  and maps it write-through cacheable; this project re-maps it **non-cacheable**
  (cleaning + invalidating the D-cache around the remap so stale cached lines
  for the array don't fool the reads), so the numbers reflect real FMC/SDRAM
  bus traffic, not L1 cache hits.
* Write: 32-bit store pattern (`0xA5A5A5A5 ^ i`) over the whole buffer, with a
  `__DSB()` drain included in the timing.
* Read: 32-bit loads summed (minimal per-word overhead); a separate untimed pass
  verifies data integrity.
* memcpy: SDRAM -> SDRAM (half buffer).
* Timing via the 1 ms SysTick (`HAL_GetTick`).

## Results (measured on hardware, 216 MHz, GCC 15.3.1)

| Operation | Size | Throughput |
| --------- | ---- | ---------- |
| Write (32-bit stores, uncached) | 8 MB | **92.0 MB/s** |
| Read (32-bit loads, uncached) | 8 MB | **23.4 MB/s** |
| memcpy (SDRAM->SDRAM) | 4 MB | **10.5 MB/s** |

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

* **Non-destructive to the app**: the test operates on its own 8 MB `.sdram`
  buffer. It only affects whatever *other* projects place in the `.sdram`
  section (e.g. the LCD framebuffer) - keep this benchmark off while other
  firmware has live data there.
* The figures are for **uncached** accesses (the benchmark's stated intent). The
  cached/write-through SDRAM path is faster but mixes in L1 hits, so it is not
  what this project reports.

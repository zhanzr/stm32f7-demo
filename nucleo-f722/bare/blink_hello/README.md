# blink_hello - minimal STM32F722ZE (NUCLEO-F722ZE) template @ 216 MHz

Toggles the three on-board LEDs (**LD1 PB0, LD2 PB7, LD3 PB14**, high active)
and prints, on every toggle, a line over **USART3 (PD8 TX / PD9 RX, AF7)** to
the ST-Link VCP at 115200: the current core frequency and the on-chip ADC
internal channels (die temperature / VREFINT / VBAT).

## Build & flash

```bash
bash build.sh          # == cmake -G Ninja .. && ninja  (GNU arm-none-eabi-gcc)
ninja flash            # probe-rs (SWD) on the on-board ST-Link V2-1
```

`ninja flash` auto-detects the probe; if several ST-Links are attached, pin
the nucleo one at configure time:

```bash
cmake -G Ninja -DDEBUG_PROBE=0483:3752:xxxx... ..   # select the nucleo probe
ninja flash
```

Open the **USART3** console (`COMxx` @ 115200 via the ST-Link VCP). Every
second you should see, e.g.:

```
=== blink_hello on STM32F722ZE @ 216000000 Hz ===
ADC: VREFINT raw=1494 cal=1499 -> VDDA=3311 mV | Temp=38.7 C | VBAT=3.331 V @ 216000000 Hz
```

Note the nucleo HSE is the ST-Link MCO (8 MHz bypass), not a crystal; see the
board README.

The board layer (clock init to 216 MHz, USART3 console, SWV/ITM, startup,
linker) is in `../../board/`; the F7 HAL + CMSIS come from the shared
STM32Cube_FW_F7 package (`../../../cmake/stm32cubef7.cmake`).

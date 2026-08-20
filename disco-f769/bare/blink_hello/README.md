# blink_hello - minimal STM32F769NI (STM32F769I-Discovery) template @ 216 MHz

Tiny project template: toggles the three on-board LEDs - **LD1 (PJ13),
LD2 (PJ5), LD3 (PA12), all high active** - and measures the ADC internal
channels, printing the **current frequency and the ADC values** once per second
over USART1 (`COMxx` @ 115200 via the on-board ST-Link V2's VCP). Everything
else - 216 MHz clock init, MPU, I/D caches, UART console, SysTick, newlib
stubs, startup + linker script - comes from the **shared board layer**
(`../board`) and toolchain helpers (`../cmake`), so a new project only needs
its own `src/main.c`.

Use this as the starting skeleton for new disco apps: copy the folder,
rename the project in `CMakeLists.txt`, and replace `src/main.c`.

## ADC internal channels

Reads the three on-chip ADC1 internal channels (see RM0410: ADC1_IN17 =
VREFINT, ADC1_IN18 = temperature sensor / VBAT, muxed):

| Channel | What it measures | Report |
| ------- | ---------------- | ------ |
| `ADC_CHANNEL_VREFINT` | internal bandgap reference (~1.2 V) | raw code + inferred **VDDA** |
| `ADC_CHANNEL_TEMPSENSOR` | die temperature sensor | ** degC** (two-point factory calibration) |
| `ADC_CHANNEL_VBAT` | battery pin, internal `/4` divider | **V** |

The physical values are computed from the factory calibration stored in the
device memory (`VREFINT_CAL` @ `0x1FF0F44A`, `TS_CAL1` @ `0x1FF0F44C` =
30  degC, `TS_CAL2` @ `0x1FF0F44E` = 110  degC, acquired at Vref+ = 3.3 V):

* `VDDA = 3300 mV x VREFINT_CAL / VREFINT_raw`
* die temp: normalize `TS_raw` to the 3.3 V reference via `VREFINT_CAL /
  VREFINT_raw`, then linear-interpolate between `TS_CAL1` and `TS_CAL2`
* `VBAT = 4 x VBAT_raw x VDDA / 4095` (channel is a `/4` divider)

Example output from the board:

```
ADC: VREFINT raw=1501 cal=1503 -> VDDA=3304 mV | Temp=44.1 C | VBAT=3.32 V @ 216000000 Hz
```

## Build

```bash
bash build.sh                      # default: GNU arm-none-eabi-gcc == cmake -G Ninja .. && ninja

# Keil AC6 (armclang) in a separate build dir (CMAKE_TOOLCHAIN_FILE is cached)
mkdir -p build-ac6 && cd build-ac6
cmake -G Ninja -DSTM32_TOOLCHAIN=armclang ..
ninja
```

`ninja` builds the `.elf` + `.hex` (the `.hex` is what `ninja flash` programs);
`ninja bin` additionally writes a raw `.bin` image "in case" you need it.

## Flash + observe

```bash
ninja flash        # probe-rs through the on-board ST-Link V2 (SWD)
```

Open `COMxx` at 115200 8-N-1 (ST-Link V2 VCP) - you should see one ADC line
per second (with the current frequency), and the three physical LEDs blinking
in sync.

## Notes

* **Polarity**: all three LEDs are **high active** - `GPIO_PIN_SET` = ON (the
  Discovery LEDs are not inverted like some mini boards).
* Clock tree: HSE 25 MHz -> PLL (M=25, N=432, P=2) -> SYSCLK 216 MHz, HCLK
  216 MHz, APB1 54 MHz, APB2 108 MHz, OverDrive on (copied verbatim from the
  vendor 216 MHz template).
* **ADC clock**: ADC1 on PCLK2 (108 MHz) / 4 = 27 MHz, 480-cycle sampling
  time, single software-triggered conversion per channel.


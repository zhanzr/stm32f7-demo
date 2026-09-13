# STM32F7 HAL + CMSIS (vendored)

Vendored (trimmed) subset of the official **STM32Cube_FW_F7 v1.17.4** package,
from:

```
C:/Users/user1/STM32Cube/Repository/STM32Cube_FW_F7_V1.17.4/Drivers
```

Only what the projects in this repo compile against is included:

| Path                                              | What it is                      |
| ------------------------------------------------- | ------------------------------- |
| `CMSIS/Include`                                   | CMSIS 5 core headers (M0..M7)   |
| `CMSIS/Device/ST/STM32F7xx/Include`               | STM32F7 device headers          |
| `STM32F7xx_HAL_Driver/Inc` (+ `Inc/Legacy`)       | STM32F7 HAL headers             |
| `STM32F7xx_HAL_Driver/Src` (+ `Src/Legacy`, all modules) | STM32F7 HAL sources      |

Not vendored: the package's BSP, DSP/NN/RTOS middleware, example projects,
user manuals (`.chm`), and docs.

To restore the full official package (e.g. for something not vendored here),
configure with `-DSTM32F7_HAL_ROOT=<root of the full package's Drivers dir>` —
see the root `README.md`. License: ST's standard SLA0044 applies to these
files.

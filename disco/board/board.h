#ifndef __BOARD_H__
#define __BOARD_H__

#include "stm32f7xx_hal.h"

/* Init: caches + 216 MHz clocks + on-board SDRAM + USART1 console + ITM. */
void Board_Init(void);
void SystemClock_Config(void);  /* HSE 25 MHz -> PLL -> 216 MHz core / 216 MHz HCLK */
void Error_Handler(void);

/* Open MPU regions for the FMC controller (0xA0000000) and the on-board SDRAM
 * (0xC0000000). Called by Board_Init; exposed for projects that re-map the
 * SDRAM (e.g. non-cacheable benchmarking) and need the FMC region re-applied. */
void MPU_OpenFmcSdram(void);

/* Signal that the on-board SDRAM is up (called by SDRAM_Init); _sbrk (in
 * syscalls.c) switches the malloc() heap over to the SDRAM only after this. */
void Board_SdramReady(void);

#endif /* __BOARD_H__ */

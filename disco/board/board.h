#ifndef __BOARD_H__
#define __BOARD_H__

#include "stm32f7xx_hal.h"

/* Init: caches + 216 MHz clocks + USART1 console (PA9/PA10) + ITM. */
void Board_Init(void);
void SystemClock_Config(void);  /* HSE 25 MHz -> PLL -> 216 MHz core / 216 MHz HCLK */
void Error_Handler(void);

#endif /* __BOARD_H__ */

#ifndef __BOARD_H__
#define __BOARD_H__

#include "stm32f7xx_hal.h"

/* Init: caches + 216 MHz clocks + USART3 console + ITM. */
void Board_Init(void);
void SystemClock_Config(void);  /* HSE 8 MHz bypass -> PLL -> 216 MHz core / 216 MHz HCLK */
void Error_Handler(void);

/* Signal that the memory the malloc() heap grows into is usable up-front.
 * On this board the heap lives in the on-chip SRAM (always available), so this
 * is a no-op kept for source compatibility with the disco board layer. */
void Board_SdramReady(void);

#endif /* __BOARD_H__ */
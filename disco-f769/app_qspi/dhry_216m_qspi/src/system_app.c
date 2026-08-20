/**
  * @file    system_app.c
  * @brief   Non-destructive system init for the disco QSPI app.
  *
  * The stock ST system_stm32f7xx.c SystemInit() resets the whole RCC clock
  * tree (turns off PLL/HSE, resets dividers). That would kill the QUADSPI
  * clock the bootloader configured, and because this app's code and vector
  * table live in the QUADSPI memory-mapped space (0x90000000), the very next
  * instruction fetch would fault.
  *
  * So the app provides its own SystemInit() that does nothing destructive, and
  * a SystemCoreClock that reflects what the bootloader set (216 MHz core). The
  * bootloader owns the clock tree; the app only uses it.
  */

#include "stm32f7xx.h"

/* Bootloader left the core at 216 MHz (PLL, HSE 25 MHz, M25 N432 P2). */
uint32_t SystemCoreClock = 216000000U;

/* Referenced by HAL_RCC_GetHCLKFreq()/GetPCLKx()Freq(). */
const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8]  = {0, 0, 0, 0, 1, 2, 3, 4};

/* Called by the reset handler before main. Deliberately leaves RCC/QUADSPI
 * untouched. */
void SystemInit(void)
{
    SystemCoreClock = 216000000U;
}

/* HAL clock helpers may call this; the clock tree is owned by the bootloader,
 * so just report the known values. */
void SystemCoreClockUpdate(void)
{
    SystemCoreClock = 216000000U;
}

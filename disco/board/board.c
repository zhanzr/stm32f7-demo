/**
  * @file    board.c
  * @brief   Board init for the STM32F769I-Discovery (STM32F769NI) benchmark
  *          builds.
  *
  * Clock tree (HSE = 25 MHz), copied verbatim from the vendor Template example
  * (216 MHz):
  *   PLL M=25 -> PLL input 1 MHz
  *   PLL N=432 -> VCO 432 MHz
  *   PLL P=2 -> SYSCLK (core clock) 216 MHz
  *   AHB prescaler 1 -> HCLK 216 MHz
  *   APB1 prescaler 4 -> 54 MHz
  *   APB2 prescaler 2 -> 108 MHz
  *   OverDrive on, VOS scale 1, flash latency 7.
  *
  * Console: USART1 (PA9 TX / PA10 RX, AF7) @ 115200 8-N-1 to the ST-Link V2
  * VCOM. I/D caches enabled. SWV/ITM is enabled in firmware as well.
  */

#include "board.h"
#include "uart_printf.h"
#include "swv_printf.h"

/* ------------------------------------------------------------------------ */
/* Same MPU setup the vendor 216 MHz builds use: a 4 GB region
 * with subregions 0/1/2/7 disabled, leaving FLASH/DTCM/SRAM on the privileged
 * default memory map (Normal, write-back, cacheable) and denying access to the
 * rest (0x60000000..0xDFFFFFFF, including the unused SDRAM / FMC space). */
static void MPU_Config(void)
{
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    HAL_MPU_Disable();

    MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
    MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress      = 0x0;
    MPU_InitStruct.Size             = MPU_REGION_SIZE_4GB;
    MPU_InitStruct.SubRegionDisable = 0x87;
    MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
    MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable      = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/* ------------------------------------------------------------------------ */
/* Clock tree as in the vendor 216 MHz template (HSE 25 MHz -> PLL M=25 N=432
 * P=2 -> 216 MHz SYSCLK, HCLK 216 MHz, APB1 54 MHz, APB2 108 MHz,
 * OverDrive on, VOS scale 1, flash latency 7). */
#ifdef QSPI_APP
/* QSPI apps: the bootloader owns the clock tree, so an empty
 * SystemClock_Config keeps the (identical) app main() callable without
 * re-configuring RCC and killing the QUADSPI clock the code runs from. */
void SystemClock_Config(void)
{
    /* no-op: disco_boot already configured HSE -> PLL -> 216 MHz + QUADSPI */
}
#else
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 25;
    RCC_OscInitStruct.PLL.PLLN       = 432;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 9;
    RCC_OscInitStruct.PLL.PLLR       = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_PWREx_EnableOverDrive() != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
    {
        Error_Handler();
    }
}
#endif /* QSPI_APP */

/* ------------------------------------------------------------------------ */
void Board_Init(void)
{
#ifndef QSPI_APP
    MPU_Config();
#else
    /* disco_boot disables IRQ before jumping to the app; re-enable it here so
     * HAL_GetTick()/HAL_Delay() (SysTick IRQ) work. */
    __enable_irq();
#endif
    SCB_EnableICache();
    SCB_EnableDCache();

    SystemClock_Config();   /* no-op for QSPI apps */
    UART_Init();
    SWV_Init();
}

/* ------------------------------------------------------------------------ */
/* HAL tick source. The startup weak-handler default is an infinite loop, so
 * without this the SysTick (enabled by HAL_Init) wedges the core the moment
 * the first tick fires. */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ------------------------------------------------------------------------ */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

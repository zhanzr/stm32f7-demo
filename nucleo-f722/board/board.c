/**
  * @file    board.c
  * @brief   Board init for the STM32F722ZE (NUCLEO-F722ZE) benchmark builds.
  *
  * Clock tree (HSE = 8 MHz, derived from the ST-Link MCO - no crystal on the
  * Nucleo board, so HSE is used in bypass mode), to 216 MHz:
  *   PLL M=8  -> PLL input 1 MHz
  *   PLL N=432 -> VCO 432 MHz
  *   PLL P=2  -> SYSCLK (core clock) 216 MHz
  *   AHB prescaler 1 -> HCLK 216 MHz
  *   APB1 prescaler 4 -> 54 MHz
  *   APB2 prescaler 2 -> 108 MHz
  *   OverDrive on, VOS scale 1, flash latency 7.
  *
  * Console: USART3 (PD8 TX / PD9 RX, AF7) @ 115200 8-N-1 to the ST-Link VCP.
  * I/D caches enabled. SWV/ITM is enabled in firmware as well.
  *
  * The on-board LEDs (PB0/PB7/PB14 - LD1/LD2/LD3, high active) are driven per
  * project (see bare/blink_hello). This board has no external SDRAM; all RAM
  * is the on-chip 256 KB (DTCM 64 KB + SRAM 192 KB).
  */

#include <stdint.h>

#include "board.h"
#include "uart_printf.h"
#include "swv_printf.h"

/* ------------------------------------------------------------------------ */
/* Standard F7 MPU setup: a 4 GB base region with subregions 0/1/2/7 disabled
 * leaves FLASH/DTCM/SRAM on the privileged default memory map (Normal,
 * write-back cacheable) and denies the rest. The F722 Nucleo has no external
 * memory, so no FMC/SDRAM regions are opened. */
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
/* Clock tree as in the vendor NUCLEO-F722ZE template (HSE 8 MHz bypass ->
 * PLL M=8 N=432 P=2 -> SYSCLK 216 MHz, HCLK 216 MHz, APB1 54 MHz, APB2 108 MHz,
 * OverDrive on, VOS scale 1, flash latency 7).
 * A project may override the core frequency by defining BOARD_PLL_N. */
#ifndef BOARD_PLL_N
#define BOARD_PLL_N   432
#endif

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* HSE comes from the ST-Link MCO (no crystal on this Nucleo): bypass mode. */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_BYPASS;
    RCC_OscInitStruct.HSIState       = RCC_HSI_OFF;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 8;
    RCC_OscInitStruct.PLL.PLLN       = BOARD_PLL_N;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 9;
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

/* ------------------------------------------------------------------------ */
void Board_Init(void)
{
    MPU_Config();
    SCB_EnableICache();
    SCB_EnableDCache();

    /* newlib's %f formatter performs an internal divide-by-zero that the
     * Cortex-M7's CCR.DIV_0_TRP trap turns into a HardFault. Disable the trap
     * so %f works (the divide just yields 0 and formatting continues). */
    SCB->CCR &= ~SCB_CCR_DIV_0_TRP_Msk;

    SystemClock_Config();   /* HSE 8 MHz bypass -> PLL -> 216 MHz */
    UART_Init();            /* USART3 (PD8 TX / PD9 RX) @ 115200 */
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
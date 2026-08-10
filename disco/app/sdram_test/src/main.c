#include <stdio.h>
#include <string.h>

#include "board.h"
#include "uart_printf.h"
#include "stm32f769i_discovery_sdram.h"

/* Benchmark region: 8 MB of the 16 MB SDRAM at 0xC0000000. */
#define SDRAM_BASE       (SDRAM_DEVICE_ADDR)          /* 0xC0000000 */
#define TEST_SIZE        (8U * 1024U * 1024U)
#define TEST_WORDS       (TEST_SIZE / 4U)

/* ------------------------------------------------------------------------ */
/* The shared board layer denies access to 0x60000000..0xDFFFFFFF (region 0).
 * Add higher-priority regions for the FMC controller (0xA0000000, device) and
 * the FMC SDRAM (0xC0000000). The SDRAM is mapped non-cacheable so the
 * benchmark measures real FMC/SDRAM bus traffic, not L1 cache hits. */
static void Periph_MPU_Enable(void)
{
    MPU_Region_InitTypeDef r = {0};

    /* FMC control registers @ 0xA0000000, 8 KB, device memory. */
    r.Enable           = MPU_REGION_ENABLE;
    r.Number           = MPU_REGION_NUMBER2;
    r.BaseAddress      = 0xA0000000;
    r.Size             = MPU_REGION_SIZE_8KB;
    r.SubRegionDisable = 0x00;
    r.TypeExtField     = MPU_TEX_LEVEL0;
    r.AccessPermission = MPU_REGION_FULL_ACCESS;
    r.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
    r.IsShareable      = MPU_ACCESS_SHAREABLE;
    r.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
    r.IsBufferable     = MPU_ACCESS_BUFFERABLE;
    HAL_MPU_ConfigRegion(&r);

    /* SDRAM @ 0xC0000000, 32 MB, NON-cacheable normal memory (C=0, B=0) so
     * the benchmark measures real FMC/SDRAM bus traffic, not L1 cache hits. */
    r.Number           = MPU_REGION_NUMBER1;
    r.BaseAddress      = SDRAM_BASE;
    r.Size             = MPU_REGION_SIZE_32MB;
    r.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
    r.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&r);
}

/* ------------------------------------------------------------------------ */
/* Timing via the 1 ms SysTick (HAL_GetTick). */
static uint32_t ms_since(uint32_t start)
{
    return HAL_GetTick() - start;
}

static const char *verify_ok(uint32_t n) { return n ? "FAIL" : "OK"; }

/* ------------------------------------------------------------------------ */
int main(void)
{
    HAL_Init();
    Board_Init();
    Periph_MPU_Enable();

    printf("\r\n=== sdram_test on STM32F769NI @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);

    if (BSP_SDRAM_Init() != SDRAM_OK)
    {
        printf("SDRAM: init FAILED\r\n");
        while (1)
        {
        }
    }
    printf("SDRAM: init OK, %lu MB @ 0x%08lX, testing %lu MB\r\n",
           (unsigned long)(SDRAM_DEVICE_SIZE / (1024U * 1024U)),
           (unsigned long)SDRAM_BASE,
           (unsigned long)(TEST_SIZE / (1024U * 1024U)));

    volatile uint32_t *p = (volatile uint32_t *)SDRAM_BASE;
    uint32_t i, word, errors, ms;
    uint32_t sum;
    double mbps;

    while (1)
    {
        /* ---- Write: 32-bit store pattern over the whole region ---- */
        uint32_t t0 = HAL_GetTick();
        for (i = 0; i < TEST_WORDS; i++)
        {
            p[i] = (uint32_t)(0xA5A5A5A5UL ^ i);
        }
        __DSB();   /* drain the store buffer so writes actually reach the SDRAM */
        ms = ms_since(t0);
        mbps = (double)(TEST_SIZE / (1024U * 1024U)) * 1000.0 / (double)ms;
        printf("Write : %lu MB in %lu ms -> %.1f MB/s\r\n",
               (unsigned long)(TEST_SIZE / (1024U * 1024U)), (unsigned long)ms, mbps);

        /* ---- Read: minimal-overhead 32-bit loads (sum) ---- */
        SCB_InvalidateDCache_by_Addr((void *)SDRAM_BASE, TEST_SIZE);
        t0 = HAL_GetTick();
        sum = 0;
        for (i = 0; i < TEST_WORDS; i++)
        {
            sum += p[i];
        }
        ms = ms_since(t0);
        mbps = (double)(TEST_SIZE / (1024U * 1024U)) * 1000.0 / (double)ms;
        printf("Read  : %lu MB in %lu ms -> %.1f MB/s (sum=0x%08lX)\r\n",
               (unsigned long)(TEST_SIZE / (1024U * 1024U)), (unsigned long)ms, mbps,
               (unsigned long)sum);

        /* ---- Verify (untimed): every word matches the write pattern ---- */
        SCB_InvalidateDCache_by_Addr((void *)SDRAM_BASE, TEST_SIZE);
        errors = 0;
        for (i = 0; i < TEST_WORDS; i++)
        {
            word = p[i];
            if (word != (uint32_t)(0xA5A5A5A5UL ^ i))
            {
                errors++;
            }
        }
        printf("Verify: %s\r\n", verify_ok(errors));

        /* ---- memcpy SDRAM -> SDRAM ---- */
        t0 = HAL_GetTick();
        memcpy((void *)(SDRAM_BASE + TEST_SIZE / 2), (const void *)SDRAM_BASE, TEST_SIZE / 2);
        __DSB();   /* drain writes before timing stops */
        ms = ms_since(t0);
        mbps = (double)(TEST_SIZE / 2 / (1024U * 1024U)) * 1000.0 / (double)ms;
        printf("memcpy: %lu MB in %lu ms -> %.1f MB/s (SDRAM->SDRAM)\r\n",
               (unsigned long)(TEST_SIZE / 2 / (1024U * 1024U)), (unsigned long)ms, mbps);

        printf("---\r\n");
        HAL_Delay(5000);
    }
}

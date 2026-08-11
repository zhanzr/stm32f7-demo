#include <stdio.h>
#include <string.h>

#include "board.h"
#include "uart_printf.h"

/* The test buffer is a plain global array in the on-board SDRAM (the `.sdram`
 * linker section, which also holds the LCD framebuffer). Since .data/.bss/heap
 * live in the SDRAM too, testing a hardcoded base (0xC0000000) would wipe the
 * app's own state - the array is the non-destructive, linker-managed way. */
#define TEST_SIZE        (8U * 1024U * 1024U)
#define TEST_WORDS       (TEST_SIZE / 4U)
static uint32_t test_buf[TEST_WORDS] __attribute__((section(".sdram")));

/* ------------------------------------------------------------------------ */
/* The shared board layer opens the FMC + SDRAM (write-through cacheable).
 * This benchmark re-maps the SDRAM NON-cacheable so it measures real
 * FMC/SDRAM bus traffic, not L1 cache hits. Clean the D-cache first so the
 * app's .data/.bss (also in the SDRAM) are flushed before the region stops
 * being cached. */
static void Periph_MPU_Enable(void)
{
    MPU_Region_InitTypeDef r = {0};

    /* Flush dirty SDRAM .data/.bss/array lines first, then re-map the SDRAM
     * NON-cacheable so the benchmark measures real FMC/SDRAM bus traffic.
     * The cache may hold valid lines for the `.sdram` array (Board_Init's
     * SDRAM_Init zeroes it), so invalidate afterwards - otherwise reads could
     * hit stale cached zeros. */
    SCB_CleanDCache();

    r.Enable           = MPU_REGION_ENABLE;
    r.Number           = MPU_REGION_NUMBER1;
    r.BaseAddress      = 0xC0000000;
    r.Size             = MPU_REGION_SIZE_32MB;
    r.SubRegionDisable = 0x00;
    r.TypeExtField     = MPU_TEX_LEVEL0;
    r.AccessPermission = MPU_REGION_FULL_ACCESS;
    r.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
    r.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
    r.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;   /* C=0, B=0 */
    r.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&r);

    SCB_InvalidateDCache();   /* drop stale lines for the now-uncached region */
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
    /* Progress markers for probe-rs debugging (bottom of DTCM, away from the
     * stack which grows down from 0x20020000). */
    volatile uint32_t *mark = (volatile uint32_t *)0x20000000;
    mark[0] = 0xDEAD0001;
    HAL_Init();
    mark[1] = 0xDEAD0002;
    Board_Init();
    mark[2] = 0xDEAD0003;
    Periph_MPU_Enable();
    mark[3] = 0xDEAD0004;

    printf("\r\n=== sdram_test on STM32F769NI @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);

    printf("SDRAM: test_buf[%lu KB] in .sdram @ 0x%08lX (non-cacheable)\r\n",
           (unsigned long)(TEST_SIZE / 1024U), (unsigned long)(uintptr_t)test_buf);

    volatile uint32_t *p = test_buf;
    uint32_t i, word, errors, ms;
    uint32_t sum;
    double mbps;

    while (1)
    {
        /* ---- Write: 32-bit store pattern over the whole buffer ---- */
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

        /* ---- memcpy SDRAM -> SDRAM (within test_buf) ---- */
        t0 = HAL_GetTick();
        memcpy(&test_buf[TEST_WORDS / 2], test_buf, TEST_SIZE / 2);
        __DSB();   /* drain writes before timing stops */
        ms = ms_since(t0);
        mbps = (double)(TEST_SIZE / 2 / (1024U * 1024U)) * 1000.0 / (double)ms;
        printf("memcpy: %lu MB in %lu ms -> %.1f MB/s (SDRAM->SDRAM)\r\n",
               (unsigned long)(TEST_SIZE / 2 / (1024U * 1024U)), (unsigned long)ms, mbps);

        printf("---\r\n");
        HAL_Delay(5000);
    }
}

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "uart_printf.h"
#include "stm32f769i_discovery_qspi.h"

/* Benchmark regions (MX25L51245G = 64 MB, subsector 4 KB, page 256 B).
 * Erase+program over 128 KB (32 subsectors); read over 1 MB for a solid
 * throughput number. */
#define TEST_ADDR        (0x000000UL)
#define ERASE_PROG_SIZE  (128U * 1024U)
#define READ_SIZE        (1U * 1024U * 1024U)
#define CHUNK_SIZE       (32U * 1024U)
#define SUBSECTOR_SIZE   (4U * 1024U)

static uint8_t tx_buf[CHUNK_SIZE];
static uint8_t rx_buf[CHUNK_SIZE];

/* ------------------------------------------------------------------------ */
/* The shared board layer denies 0x60000000..0xDFFFFFFF (region 0), which
 * covers the QUADSPI peripheral at 0xA0001000. Add a higher-priority 8 KB
 * region at 0xA0000000 (device memory) - same as the vendor template's FMC
 * control-register region, which also spans the QUADSPI block. */
static void QSPI_MPU_Enable(void)
{
    MPU_Region_InitTypeDef r = {0};

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
    r.IsBufferable     = MPU_ACCESS_BUFFERABLE;   /* device memory */

    HAL_MPU_ConfigRegion(&r);
}

/* Fill a chunk with a deterministic pattern derived from (addr, i). */
static void fill_pattern(uint8_t *p, uint32_t size, uint32_t base_addr, uint32_t i)
{
    uint32_t n;
    for (n = 0; n < size; n++)
    {
        p[n] = (uint8_t)((base_addr + n) ^ i);
    }
}

/* ------------------------------------------------------------------------ */
static void run_benchmark(void)
{
    QSPI_Info info;
    uint32_t i, n_subs = ERASE_PROG_SIZE / SUBSECTOR_SIZE, n_chunks = ERASE_PROG_SIZE / CHUNK_SIZE,
             n_read = READ_SIZE / CHUNK_SIZE;
    uint32_t t0, ms;

    BSP_QSPI_GetInfo(&info);
    printf("QSPI: MX25L51245G, flash %lu MB, subsector %lu B, page %lu B\r\n",
           (unsigned long)(info.FlashSize / (1024U * 1024U)),
           (unsigned long)info.EraseSectorSize,
           (unsigned long)info.ProgPageSize);

    /* ---- Erase (subsector 4 KB, one per erase call) ---- */
    t0 = HAL_GetTick();
    for (i = 0; i < n_subs; i++)
    {
        if (BSP_QSPI_Erase_Block(TEST_ADDR + i * SUBSECTOR_SIZE) != QSPI_OK)
        {
            printf("QSPI: erase error @ 0x%08lX\r\n", (unsigned long)(TEST_ADDR + i * SUBSECTOR_SIZE));
            return;
        }
    }
    ms = HAL_GetTick() - t0;
    printf("Erase  : %lu KB in %lu ms -> %lu KB/s (%.2f ms/subsector)\r\n",
           (unsigned long)(ERASE_PROG_SIZE / 1024U), (unsigned long)ms,
           (unsigned long)((uint64_t)(ERASE_PROG_SIZE / 1024U) * 1000U / ms),
           (double)ms / (double)n_subs);

    /* ---- Program (page 256 B, 4-wire QPI) ---- */
    t0 = HAL_GetTick();
    for (i = 0; i < n_chunks; i++)
    {
        fill_pattern(tx_buf, CHUNK_SIZE, TEST_ADDR + i * CHUNK_SIZE, i);
        if (BSP_QSPI_Write(tx_buf, TEST_ADDR + i * CHUNK_SIZE, CHUNK_SIZE) != QSPI_OK)
        {
            printf("QSPI: program error @ 0x%08lX\r\n", (unsigned long)(TEST_ADDR + i * CHUNK_SIZE));
            return;
        }
    }
    ms = HAL_GetTick() - t0;
    printf("Program: %lu KB in %lu ms -> %lu KB/s\r\n",
           (unsigned long)(ERASE_PROG_SIZE / 1024U), (unsigned long)ms,
           (unsigned long)((uint64_t)(ERASE_PROG_SIZE / 1024U) * 1000U / ms));

    /* ---- Read + verify (indirect, FIFO), 1 MB ---- */
    t0 = HAL_GetTick();
    for (i = 0; i < n_read; i++)
    {
        if (BSP_QSPI_Read(rx_buf, TEST_ADDR + i * CHUNK_SIZE, CHUNK_SIZE) != QSPI_OK)
        {
            printf("QSPI: read error @ 0x%08lX\r\n", (unsigned long)(TEST_ADDR + i * CHUNK_SIZE));
            return;
        }
        if (i < n_chunks)
        {
            fill_pattern(tx_buf, CHUNK_SIZE, TEST_ADDR + i * CHUNK_SIZE, i);
            if (memcmp(rx_buf, tx_buf, CHUNK_SIZE) != 0)
            {
                printf("QSPI: VERIFY FAILED @ chunk %lu\r\n", (unsigned long)i);
                return;
            }
        }
    }
    ms = HAL_GetTick() - t0;
    printf("Read   : %lu KB in %lu ms -> %lu KB/s (%.2f MB/s) [verify OK]\r\n",
           (unsigned long)(READ_SIZE / 1024U), (unsigned long)ms,
           (unsigned long)((uint64_t)(READ_SIZE / 1024U) * 1000U / ms),
           (double)(READ_SIZE / 1024U) / 1024.0 * 1000.0 / (double)ms);
}

/* ------------------------------------------------------------------------ */
int main(void)
{
    HAL_Init();
    Board_Init();
    QSPI_MPU_Enable();

    printf("\r\n=== qspi_flash_test on STM32F769NI @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);

    if (BSP_QSPI_Init() != QSPI_OK)
    {
        printf("QSPI: init FAILED\r\n");
        while (1)
        {
        }
    }
    printf("QSPI: init OK\r\n");

    while (1)
    {
        run_benchmark();
        printf("---\r\n");
        HAL_Delay(5000);
    }
}

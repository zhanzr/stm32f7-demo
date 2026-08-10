/**
 * probers_alg - run the QUADSPI flash algorithm's exact register-level code as
 * a normal, debuggable firmware (the "host" that simulates probe-rs loading the
 * algorithm). Prints every JEDEC/read/program/erase result so the QUADSPI
 * behaviour can be seen instead of returning cryptic error codes.
 *
 * For the F7 QUADSPI:
 *   - QUADSPI @ 0xA0001000, RCC AHB3ENR bit 1 (QSPI clock)
 *   - pins: PB2 CLK AF9, PB6 manual CS (GPIO), PC9 IO0 AF9, PC10 IO1 AF9
 */

#include <stdio.h>
#include "board.h"
#include "uart_printf.h"
#include "stm32f7xx_hal.h"

/* ------------------------------------------------------------------------ */
/* Same register-level functions as flash_mx25l512_qspi.c. */
static void qspi_wait_busy(void)
{
    unsigned long t = 0xFFFFFu;
    while (t--) { if (!(QUADSPI->SR & 0x00000020u)) { return; } }
}

static void qspi_wait_tc(void)
{
    unsigned long t = 0xFFFFFu;
    while (t-- && !(QUADSPI->SR & 0x00000002u)) { }
}

static void qspi_abort(void)
{
    unsigned long t;
    if (QUADSPI->CR & 1u)
    {
        QUADSPI->CR |= 2u;
        t = 0xFFFFFu;
        while (t-- && (QUADSPI->CR & 2u)) { }
        QUADSPI->FCR = 0x1Fu;
        t = 0xFFFFFu;
        while (t-- && (QUADSPI->SR & 0x20u)) { }
    }
}

static void qspi_clear_flags(void)
{
    QUADSPI->FCR = 0x1Fu;
}

static void qspi_transfer(unsigned char instr, int use_addr, unsigned long off,
                          unsigned char *data, unsigned long n, int is_read,
                          unsigned long imode)
{
    unsigned long ccr = (unsigned long)instr | ((imode == 4u ? 3u : 1u) << 8);
    unsigned long i;
    qspi_wait_busy();
    qspi_clear_flags();
    GPIOB->ODR &= ~(1UL << 6);
    if (use_addr) ccr |= (1u << 10) | (2u << 12);
    if (n > 0u) { QUADSPI->DLR = n - 1u; ccr |= (1u << 24); }
    else        { QUADSPI->DLR = 0; }
    ccr |= ((unsigned long)(is_read ? 1 : 0)) << 26;
    if (use_addr) { QUADSPI->AR = off; }
    QUADSPI->CCR = ccr;
    if (use_addr) { QUADSPI->AR = off; } else { QUADSPI->AR = 0; }
    if (n == 0u)
    {
        qspi_wait_tc();
        qspi_clear_flags();
        GPIOB->ODR |= (1UL << 6);
        return;
    }
    if (is_read)
    {
        for (i = 0; i < n; i++)
        {
            unsigned long t = 0xFFFFFu;
            while (t-- && !(QUADSPI->SR & (0x04u | 0x02u))) { }
            data[i] = *(volatile unsigned char *)&QUADSPI->DR;
        }
        qspi_wait_tc();
        qspi_clear_flags();
        while (((QUADSPI->SR >> 8) & 0x3Fu) != 0u)
        {
            (void)*(volatile unsigned char *)&QUADSPI->DR;
        }
        qspi_clear_flags();
    }
    else
    {
        for (i = 0; i < n; i++)
        {
            unsigned long t = 0xFFFFFu;
            while (t-- && !(QUADSPI->SR & 0x04u)) { }
            *(volatile unsigned char *)&QUADSPI->DR = data[i];
        }
        qspi_wait_tc();
        qspi_clear_flags();
    }
    qspi_abort();
    GPIOB->ODR |= (1UL << 6);
}

static void qspi_cmd(unsigned char instr) { qspi_transfer(instr, 0, 0, 0, 0, 0, 1u); }
static void qspi_cmd_qpi(unsigned char instr) { qspi_transfer(instr, 0, 0, 0, 0, 0, 4u); }
static void qspi_cmd_addr(unsigned char instr, unsigned long off) { qspi_transfer(instr, 1, off, 0, 0, 0, 1u); }
static void qspi_read_n(unsigned long off, unsigned char *out, unsigned long n) { qspi_transfer(0x03, 1, off, out, n, 1, 1u); }
static void qspi_read_cmd_n(unsigned char instr, unsigned char *out, unsigned long n) { qspi_transfer(instr, 0, 0, out, n, 1, 1u); }
static void qspi_write_n(unsigned long off, const unsigned char *buf, unsigned long n) { qspi_transfer(0x02, 1, off, (unsigned char *)buf, n, 0, 1u); }

static unsigned char mx_status(void) { unsigned char s; qspi_read_cmd_n(0x05, &s, 1); return s; }
static unsigned long mx_read_id(void) { unsigned char b[3]; qspi_read_cmd_n(0x9F, b, 3u); return ((unsigned long)b[0] << 16) | ((unsigned long)b[1] << 8) | b[2]; }

/* ------------------------------------------------------------------------ */
/* Bit-bang JEDEC read (sanity check the flash + pins are alive). F7 pins:
 * PB2 SCK, PB6 NCS, PC9 MOSI, PC10 MISO. */
#define NCS   (1UL << 6)
#define SCK   (1UL << 2)
#define MOSI  (1UL << 9)
#define MISO  (1UL << 10)
static void b_delay(void) { volatile unsigned long i; for (i = 0; i < 100; i++) { } }
static void b_bb_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIOB->MODER   = 0x00001010UL;   /* PB2 out, PB6 out */
    GPIOB->OTYPER  = 0;
    GPIOB->OSPEEDR = 0x00003030UL;
    GPIOB->PUPDR   = 0;
    GPIOB->ODR     = (GPIOB->ODR | NCS) & ~SCK;
    GPIOC->MODER   = 0x00040000UL;   /* PC9 out, PC10 in */
    GPIOC->OTYPER  = 0;
    GPIOC->OSPEEDR = 0x000C0000UL;
    GPIOC->PUPDR   = 0x00200000UL;   /* PC10 pull-DOWN so MISO reads 0 when idle */
    GPIOC->ODR     = GPIOC->ODR & ~MOSI;
}
static void b_start(void) { GPIOB->ODR &= ~NCS; b_delay(); }
static void b_stop(void)  { b_delay(); GPIOB->ODR |= NCS; b_delay(); }
static void b_wr(unsigned char b)
{
    unsigned long i;
    for (i = 8; i > 0; i--)
    {
        if (b & 0x80u) { GPIOC->ODR |= MOSI; } else { GPIOC->ODR &= ~MOSI; }
        b_delay();
        GPIOB->ODR |= SCK;
        b_delay();
        GPIOB->ODR &= ~SCK;
        b_delay();
        b <<= 1;
    }
}
static unsigned char b_rd(void)
{
    unsigned char v = 0;
    unsigned long i;
    for (i = 8; i > 0; i--)
    {
        GPIOB->ODR |= SCK;
        b_delay();
        v = (unsigned char)((v << 1) | (((GPIOC->IDR & MISO) != 0) ? 1u : 0u));
        GPIOB->ODR &= ~SCK;
        b_delay();
    }
    return v;
}
static unsigned long bb_read_id(void)
{
    unsigned long r;
    b_start(); b_wr(0x66); b_stop(); b_delay();   /* reset enable (SPI) */
    b_start(); b_wr(0x99); b_stop(); b_delay();   /* reset memory (SPI) */
    b_start(); b_wr(0x9F);
    r = ((unsigned long)b_rd() << 16) | ((unsigned long)b_rd() << 8) | b_rd();
    b_stop();
    return r;
}

/* ------------------------------------------------------------------------ */
/* Same functions as flash_mx25l512_qspi.c's Init sequence. */
static void qspi_hw_init(void)
{
    __HAL_RCC_QSPI_CLK_ENABLE();
    __HAL_RCC_QSPI_FORCE_RESET();
    __HAL_RCC_QSPI_RELEASE_RESET();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIOB->MODER   = 0x00001020UL;   /* PB2 AF, PB6 out(01) manual CS */
    GPIOB->OTYPER  = 0;
    GPIOB->OSPEEDR = 0x00003030UL;
    GPIOB->PUPDR   = 0x00001000UL;   /* PB6 pull-up */
    GPIOB->AFR[0]  = 0x00000900UL;   /* PB2 AF9 (CLK) */
    GPIOB->AFR[1]  = 0;
    GPIOB->ODR     = (GPIOB->ODR | NCS);   /* CS idle high */
    GPIOC->MODER   = 0x00280000UL;   /* PC9/PC10 AF */
    GPIOC->OSPEEDR = 0x00280000UL;
    GPIOC->PUPDR   = 0;
    GPIOC->AFR[1]  = 0x00000990UL;   /* PC9/PC10 AF9 (IO0/IO1) */
    GPIOD->MODER   = 0x08000000UL;   /* PD13 AF */
    GPIOD->OSPEEDR = 0x0C000000UL;
    GPIOD->PUPDR   = 0;
    GPIOD->AFR[1]  = 0x00900000UL;   /* PD13 AF9 (IO3) */
    GPIOE->MODER   = 0x00000020UL;   /* PE2 AF */
    GPIOE->OSPEEDR = 0x00000030UL;
    GPIOE->PUPDR   = 0;
    GPIOE->AFR[0]  = 0x00000900UL;   /* PE2 AF9 (IO2) */

    QUADSPI->CR  &= ~1u;
    QUADSPI->CR   = (3u << 8);
    QUADSPI->CR   = (3u << 24);
    QUADSPI->DCR  = (25u << 16);
    QUADSPI->ABR  = 0;
    QUADSPI->CR  |= 1u;
}

/* ------------------------------------------------------------------------ */
int main(void)
{
    unsigned long id;
    unsigned long i;

    HAL_Init();
    Board_Init();

    /* The shared board MPU denies 0xA0000000 (QUADSPI registers); add a
     * higher-priority device region so the harness can touch them. */
    {
        MPU_Region_InitTypeDef m = {0};
        m.Enable           = MPU_REGION_ENABLE;
        m.Number           = MPU_REGION_NUMBER2;
        m.BaseAddress      = 0xA0000000u;
        m.Size             = MPU_REGION_SIZE_8KB;
        m.SubRegionDisable = 0;
        m.TypeExtField     = MPU_TEX_LEVEL0;
        m.AccessPermission = MPU_REGION_FULL_ACCESS;
        m.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
        m.IsShareable      = MPU_ACCESS_SHAREABLE;
        m.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
        m.IsBufferable     = MPU_ACCESS_BUFFERABLE;
        HAL_MPU_ConfigRegion(&m);
    }

    printf("\r\n=== QSPI algorithm test ===\r\n");

    while (1)
    {
        unsigned char jed[3], s, m[16];
        unsigned long w;

        /* 1. bit-bang sanity: flash should be in QPI mode here (boot left it) */
        b_bb_init();
        printf("1) BITBANG JEDEC=0x%06lX (flash in QPI -> no response)\r\n", bb_read_id());

        /* 2. QSPI hardware init */
        qspi_hw_init();
        printf("2) QSPI hw: CR=0x%08lX DCR=0x%08lX\r\n",
               (unsigned long)QUADSPI->CR, (unsigned long)QUADSPI->DCR);

        /* 3. reset the flash out of QPI: 4-line 0x66/0x99 then 1-line 0x66/0x99 */
        printf("3) reset: QPI(4-line)+SPI(1-line) 0x66/0x99 ...\r\n");
        qspi_cmd_qpi(0x66);
        qspi_cmd_qpi(0x99);
        qspi_cmd(0x66);
        qspi_cmd(0x99);
        for (i = 0; i < 200000u; i++) { }
        printf("   done\r\n");

        /* 4. now bit-bang again: did the reset bring it back to SPI? */
        b_bb_init();
        printf("4) BITBANG JEDEC after reset=0x%06lX (0xC22538 => SPI OK)\r\n", bb_read_id());

        /* 5. QUADSPI 1-line JEDEC + status + data read */
        qspi_hw_init();
        qspi_read_cmd_n(0x9F, jed, 3u);
        printf("5) QSPI JEDEC bytes=%02X %02X %02X\r\n", jed[0], jed[1], jed[2]);
        qspi_read_cmd_n(0x05, &s, 1u);
        printf("   status=0x%02X\r\n", (unsigned)s);
        qspi_read_n(0, m, 8);
        printf("   0x03[0..7]=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
               m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7]);

        /* word-DR JEDEC variant */
        qspi_wait_busy();
        qspi_clear_flags();
        GPIOB->ODR &= ~(1UL << 6);
        QUADSPI->DLR = 3u;
        QUADSPI->CCR = 0x9Fu | (1u << 8) | (1u << 24) | (1u << 26);
        QUADSPI->AR  = 0;
        QUADSPI->AR  = 0;
        for (i = 0; i < 4; i++) { unsigned long t = 0xFFFFFu; while (t-- && !(QUADSPI->SR & 0x06u)) { } w = *(volatile unsigned long *)&QUADSPI->DR; }
        qspi_wait_tc();
        qspi_clear_flags();
        GPIOB->ODR |= (1UL << 6);
        printf("   word-DR JEDEC=0x%08lX\r\n", (unsigned long)w);

        printf("---\r\n");
        HAL_Delay(5000);
    }
}

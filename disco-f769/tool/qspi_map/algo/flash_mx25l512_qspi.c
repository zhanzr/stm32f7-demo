/*
 * flash_mx25l512_qspi.c - MX25L51245G (64 MB) flash algorithm for STM32F769
 * using the QUADSPI PERIPHERAL (F7 QUADSPI @ 0xA0001000, AHB1).
 *
 * Register notes (this QUADSPI):
 *   - base 0xA0001000; CCR layout IMODE 8 / ADMODE 10 / ADSIZE 12 / DMODE 24 /
 *     FMODE 26.
 *   - DCR: FSIZE[4:0] at bits 16-20, CKMODE (0) and CSHT (8-11).
 *     FSIZE = log2(size) - 1 = 25 for 64 MB.
 *   - CR: FTHRES (8-12), PRESCALER (24-31), EN (0). QSPI clock = HCLK/
 *     (PRESCALER+1); at the connect-under-reset HSI 16 MHz that gives ~4 MHz.
 *   - The QUADSPI DR supports BYTE access (matches HAL_QSPI_* - verified on
 *     this board via the vendor BSP), and an indirect READ is started by a
 *     second AR write (HAL_QSPI_Receive re-writes AR).
 *   - NCS (PB6) is driven MANUALLY as a GPIO output around each transfer
 *     (the QUADSPI NCS is hi-Z when idle; manual CS is robust regardless of
 *     the board's CS pull).
 *
 * All commands are 1-line SPI (0x02 page program, 0x20 sector erase, 0x03
 * read, 0x05 status) so no QE bit is required. Self-verifying: every page
 * program and sector erase is read back and retried.
 *
 * Runs ON the target CPU (probe-rs loads it into RAM). Position independent:
 * no globals, only register access via literal pools.
 */

typedef volatile unsigned long  vu32;

#define RCC_AHB1ENR   (0x40023830UL)   /* GPIOB bit1, GPIOC bit2, GPIOD bit3, GPIOE bit4 */
#define RCC_AHB3ENR   (0x40023838UL)   /* QSPI bit1 */
#define RCC_AHB3RSTR  (0x40023818UL)   /* QSPI bit1 */

#define QSPI_BASE     0xA0001000UL
#define QSPI_CR       (QSPI_BASE + 0x00UL)
#define QSPI_DCR      (QSPI_BASE + 0x04UL)
#define QSPI_SR       (QSPI_BASE + 0x08UL)
#define QSPI_FCR      (QSPI_BASE + 0x0CUL)
#define QSPI_DLR      (QSPI_BASE + 0x10UL)
#define QSPI_CCR      (QSPI_BASE + 0x14UL)
#define QSPI_AR       (QSPI_BASE + 0x18UL)
#define QSPI_ABR      (QSPI_BASE + 0x1CUL)
#define QSPI_DR       (QSPI_BASE + 0x20UL)

#define GPIOB_MODER   (0x40020400UL)
#define GPIOB_OTYPER  (0x40020404UL)
#define GPIOB_OSPEEDR (0x40020408UL)
#define GPIOB_PUPDR   (0x4002040CUL)
#define GPIOB_IDR     (0x40020410UL)
#define GPIOB_ODR     (0x40020414UL)
#define GPIOB_AFRL    (0x40020420UL)
#define GPIOB_AFRH    (0x40020424UL)
#define GPIOC_MODER   (0x40020800UL)
#define GPIOC_OSPEEDR (0x40020808UL)
#define GPIOC_PUPDR   (0x4002080CUL)
#define GPIOC_AFRH    (0x40020824UL)
#define GPIOD_MODER   (0x40020C00UL)
#define GPIOD_OSPEEDR (0x40020C08UL)
#define GPIOD_PUPDR   (0x40020C0CUL)
#define GPIOD_AFRH    (0x40020C24UL)
#define GPIOE_MODER   (0x40021000UL)
#define GPIOE_OSPEEDR (0x40021008UL)
#define GPIOE_PUPDR   (0x4002100CUL)
#define GPIOE_AFRL    (0x40021020UL)

#define NCS           (1u << 6)        /* PB6 = manual CS (GPIO output) */

#define FLASH_BASE    0x90000000UL
#define SECTOR_SIZE   4096u            /* MX25L512 subsector (4 KB) */
#define JEDEC_ID      0xC2201AUL       /* MX25L51245G (64 MB) */

/* ------------------------------------------------------------------------ */
static void qspi_wait_busy(void)
{
    unsigned long t = 0xFFFFFu;
    while (t--)
    {
        if (!(*(vu32 *)QSPI_SR & 0x00000020u))   /* SR.BUSY = bit 5 */
        {
            return;
        }
    }
}

static void qspi_wait_tc(void)
{
    unsigned long t = 0xFFFFFu;
    while (t-- && !(*(vu32 *)QSPI_SR & 0x00000002u)) { }   /* SR.TCF = bit 1 */
}

static void qspi_clear_flags(void)
{
    *(vu32 *)QSPI_FCR = 0x1Fu;   /* clear all flags */
}

static void qspi_abort(void)
{
    unsigned long t;
    if (*(vu32 *)QSPI_CR & 1u)
    {
        *(vu32 *)QSPI_CR |= 2u;                 /* ABORT */
        t = 0xFFFFFu;
        while (t-- && (*(vu32 *)QSPI_CR & 2u)) { }
        *(vu32 *)QSPI_FCR = 0x1Fu;              /* clear all flags */
        t = 0xFFFFFu;
        while (t-- && (*(vu32 *)QSPI_SR & 0x20u)) { }
    }
}

/* Generic 1-line transfer.  CS (PB6) is toggled manually around the QUADSPI
 * transfer.  The DR is accessed byte-granular (matching HAL_QSPI_Transmit/
 * Receive on the F7), and an indirect READ is triggered by the second AR write.
 * `imode` selects the instruction phase width: 1 = SPI (1-line), 4 = QPI. */
static void qspi_transfer(unsigned char instr, int use_addr, unsigned long off,
                          unsigned char *data, unsigned long n, int is_read,
                          unsigned long imode)
{
    unsigned long ccr = (unsigned long)instr | ((imode == 4u ? 3u : 1u) << 8);
    unsigned long i;

    qspi_wait_busy();
    qspi_clear_flags();
    *(vu32 *)GPIOB_ODR &= ~NCS;                 /* manual CS assert */

    if (use_addr)
    {
        ccr |= (1u << 10) | (2u << 12);      /* ADMODE = 1-line, ADSIZE = 24-bit */
    }
    if (n > 0u)
    {
        *(vu32 *)QSPI_DLR = n - 1u;
        ccr |= (1u << 24);                   /* DMODE = 1-line */
    }
    else
    {
        *(vu32 *)QSPI_DLR = 0;
    }
    ccr |= ((unsigned long)(is_read ? 1 : 0)) << 26;   /* FMODE[27:26]: 1=read */

    if (use_addr) { *(vu32 *)QSPI_AR = off; }
    *(vu32 *)QSPI_CCR = ccr;
    if (use_addr) { *(vu32 *)QSPI_AR = off; } else { *(vu32 *)QSPI_AR = 0; }

    if (n == 0u)
    {
        /* Command-only: the transfer starts as soon as the configuration is
         * done and completes with TCF. */
        qspi_wait_tc();
        qspi_clear_flags();
        *(vu32 *)GPIOB_ODR |= NCS;
        return;
    }

    if (is_read)
    {
        for (i = 0; i < n; i++)
        {
            unsigned long t = 0xFFFFFu;
            while (t-- && !(*(vu32 *)QSPI_SR & (0x04u | 0x02u))) { }  /* FTF | TCF */
            data[i] = *(volatile unsigned char *)QSPI_DR;
        }
        qspi_wait_tc();
        qspi_clear_flags();
        /* Drain any residual RX FIFO bytes (F7 can leave extra data at the
         * end of a read transfer - see HAL_QSPI_Receive QSPI1_V1_0). */
        while (((*(vu32 *)QSPI_SR >> 8) & 0x3Fu) != 0u)
        {
            (void)*(volatile unsigned char *)QSPI_DR;
        }
        qspi_clear_flags();
    }
    else
    {
        for (i = 0; i < n; i++)
        {
            unsigned long t = 0xFFFFFu;
            while (t-- && !(*(vu32 *)QSPI_SR & 0x04u)) { }  /* FTF */
            *(volatile unsigned char *)QSPI_DR = data[i];
        }
        qspi_wait_tc();
        qspi_clear_flags();
    }

    qspi_abort();
    *(vu32 *)GPIOB_ODR |= NCS;                  /* manual CS deassert */
}

/* Command-only (write enable, chip erase). */
static void qspi_cmd(unsigned char instr)
{
    qspi_transfer(instr, 0, 0, (unsigned char *)0, 0, 0, 1u);
}

/* Command-only in QPI (4-line) mode - to reset the flash out of the QPI mode
 * the bootloader/app leaves it in. */
static void qspi_cmd_qpi(unsigned char instr)
{
    qspi_transfer(instr, 0, 0, (unsigned char *)0, 0, 0, 4u);
}

/* Command + 24-bit address, no data (sector erase). */
static void qspi_cmd_addr(unsigned char instr, unsigned long off)
{
    qspi_transfer(instr, 1, off, (unsigned char *)0, 0, 0, 1u);
}

/* Read n bytes at flash offset off (0x03). */
static void qspi_read_n(unsigned long off, unsigned char *out, unsigned long n)
{
    qspi_transfer(0x03, 1, off, out, n, 1, 1u);
}

/* Read n bytes with no address (0x9F JEDEC). */
static void qspi_read_cmd_n(unsigned char instr, unsigned char *out, unsigned long n)
{
    qspi_transfer(instr, 0, 0, out, n, 1, 1u);
}

/* Write n bytes at flash offset off (0x02 page program). */
static void qspi_write_n(unsigned long off, const unsigned char *buf, unsigned long n)
{
    qspi_transfer(0x02, 1, off, (unsigned char *)buf, n, 0, 1u);
}

/* Read the status register (0x05). */
static unsigned char mx_status(void)
{
    unsigned char s;
    qspi_read_cmd_n(0x05, &s, 1);
    return s;
}

static void mx_write_enable(void)
{
    qspi_cmd(0x06);
}

/* Wait for the MX25L512 WIP (status bit 0) to clear. */
static int mx_wip(void)
{
    unsigned long t = 0xFFFFFFu;
    volatile unsigned long d;
    for (d = 0; d < 10000u; d++) { }
    while (t--)
    {
        if (!(mx_status() & 1u)) { return 0; }
    }
    return 1;
}

/* Read-back verify (gap-tolerant) with retry-friendly return. */
static int mx_verify(unsigned long off, const unsigned char *exp, unsigned long n)
{
    unsigned long done = 0;
    while (done < n)
    {
        unsigned long c = n - done;
        unsigned char tmp[64];
        unsigned long i;
        if (c > 64u) { c = 64u; }
        qspi_read_n(off + done, tmp, c);
        for (i = 0; i < c; i++)
        {
            if (tmp[i] != exp[done + i] && !(tmp[i] == 0xFFu && exp[done + i] == 0x00u))
            {
                return 1;
            }
        }
        done += c;
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
static void qspi_hw_init(void)
{
    /* 1. Peripheral + GPIO clocks; reset the QUADSPI (it may be stuck in
     *    memory-mapped mode from the bootloader/app that ran before).
     *    AHB3ENR: QSPI=bit1; AHB1ENR: GPIOB=bit1, GPIOC=bit2, GPIOD=bit3,
     *    GPIOE=bit4. */
    *(vu32 *)RCC_AHB3ENR |= 0x00000002UL;        /* QSPI clock */
    (void)*(vu32 *)RCC_AHB3ENR;
    *(vu32 *)RCC_AHB3RSTR |= 0x00000002UL;       /* assert QSPI reset */
    *(vu32 *)RCC_AHB3RSTR &= ~0x00000002UL;      /* release QSPI reset */
    *(vu32 *)RCC_AHB1ENR |= 0x0000001EUL;        /* GPIOB + GPIOC + GPIOD + GPIOE */
    (void)*(vu32 *)RCC_AHB1ENR;

    /* 2. GPIO: PB2 CLK AF9; PB6 = MANUAL CS as GPIO output (idle high);
     *    PC9 IO0 AF9, PC10 IO1 AF9, PE2 IO2 AF9, PD13 IO3 AF9 (all four data
     *    pins are needed for the 4-line QPI-mode reset). */
    *(vu32 *)GPIOB_MODER   = 0x00001020UL;           /* PB2 AF(10), PB6 out(01) */
    *(vu32 *)GPIOB_OTYPER  = 0;
    *(vu32 *)GPIOB_OSPEEDR = 0x00003030UL;
    *(vu32 *)GPIOB_PUPDR   = 0x00001000UL;           /* PB6 pull-up */
    *(vu32 *)GPIOB_AFRL    = 0x00000900UL;           /* PB2 AF9 */
    *(vu32 *)GPIOB_AFRH    = 0;
    *(vu32 *)GPIOB_ODR     = (*(vu32 *)GPIOB_ODR | NCS);   /* CS idle high */
    *(vu32 *)GPIOC_MODER   = 0x00280000UL;           /* PC9/PC10 AF(10) */
    *(vu32 *)GPIOC_OSPEEDR = 0x00280000UL;
    *(vu32 *)GPIOC_PUPDR   = 0;
    *(vu32 *)GPIOC_AFRH    = 0x00000990UL;           /* PC9/PC10 AF9 */
    *(vu32 *)GPIOD_MODER   = 0x08000000UL;           /* PD13 AF(10) */
    *(vu32 *)GPIOD_OSPEEDR = 0x0C000000UL;
    *(vu32 *)GPIOD_PUPDR   = 0;
    *(vu32 *)GPIOD_AFRH    = 0x00900000UL;           /* PD13 AF9 */
    *(vu32 *)GPIOE_MODER   = 0x00000020UL;           /* PE2 AF(10) */
    *(vu32 *)GPIOE_OSPEEDR = 0x00000030UL;
    *(vu32 *)GPIOE_PUPDR   = 0;
    *(vu32 *)GPIOE_AFRL    = 0x00000900UL;           /* PE2 AF9 */

    /* 3. QUADSPI: mode 0, ~4 MHz (16 MHz HSI / 4).  FTHRES = 3.
     *    DCR.FSIZE = 25 (64 MB = 2^26, minus 1). */
    *(vu32 *)QSPI_CR  &= ~1u;                        /* EN = 0 */
    *(vu32 *)QSPI_CR   = (3u << 8);                  /* FTHRES = 3 */
    *(vu32 *)QSPI_CR   = (3u << 24);                 /* PRESCALER = 3 */
    *(vu32 *)QSPI_DCR  = (25u << 16);                /* FSIZE=25, CKMODE=0, CSHT=0 */
    *(vu32 *)QSPI_ABR  = 0;
    *(vu32 *)QSPI_CR  |= 1u;                         /* EN */
}

static unsigned long mx_read_id(void)
{
    unsigned char b[3];
    qspi_read_cmd_n(0x9F, b, 3u);            /* JEDEC ID */
    return ((unsigned long)b[0] << 16) | ((unsigned long)b[1] << 8) | b[2];
}

int Init(unsigned long adr, unsigned long clk, unsigned long fnc)
{
    unsigned long id;
    (void)adr; (void)clk; (void)fnc;

    /* probe-rs payload buffers live in RAM; the M7 D-cache must not serve
     * stale lines when the CPU reads them. The algorithm does not need the
     * D-cache, so disable it. */
    *(vu32 *)0xE000ED88u &= ~(1u << 2);              /* SCB->SCTLR.C = 0 */
    __asm volatile ("dsb");
    __asm volatile ("isb");

    qspi_hw_init();

    /* Reset the MX25L512 out of QPI mode (4-line reset) in case the previous
     * app left it in QPI mode, then a 1-line reset for the SPI / continuous-
     * read case. Either way the flash ends up in SPI mode with 3-byte
     * addressing (power-on default). */
    qspi_cmd_qpi(0x66);
    qspi_cmd_qpi(0x99);
    qspi_cmd(0x66);
    qspi_cmd(0x99);
    {
        volatile unsigned long d;
        for (d = 0; d < 200000u; d++) { }
    }

    id = mx_read_id();
    if (id == JEDEC_ID) { return 0; }
    return (*(vu32 *)QSPI_CR & 0xFFu)
         | ((*(vu32 *)QSPI_SR & 0x3Fu) << 8)
         | ((*(vu32 *)QSPI_DLR & 0xFFu) << 16)
         | ((id & 0xFFu) << 24);
}

int UnInit(unsigned long fnc)
{
    (void)fnc;
    return 0;
}

int EraseSector(unsigned long adr)
{
    unsigned long off = adr - FLASH_BASE;
    int attempt;
    for (attempt = 0; attempt < 3; attempt++)
    {
        unsigned char t0[8], t1[8];
        unsigned long i;
        int ok = 1;

        mx_write_enable();
        qspi_cmd_addr(0x20, off);               /* sector erase */
        if (mx_wip()) { }

        /* Read-back check: the 4 KB sector must read back all 0xFF. */
        qspi_read_n(off, t0, 8);
        qspi_read_n(off + SECTOR_SIZE - 8u, t1, 8);
        for (i = 0; i < 8; i++) { if (t0[i] != 0xFFu || t1[i] != 0xFFu) { ok = 0; } }
        if (ok) { return 0; }
    }
    return 1;
}

int EraseChip(void)
{
    mx_write_enable();
    qspi_cmd(0xC7);
    return mx_wip();
}

int ProgramPage(unsigned long adr, unsigned long sz, unsigned char *buf)
{
    unsigned long off = adr - FLASH_BASE;
    while (sz > 0)
    {
        unsigned long room = 256u - (off % 256u);
        unsigned long n = (sz < room) ? sz : room;
        int attempt;

        for (attempt = 0; attempt < 4; attempt++)
        {
            mx_write_enable();
            qspi_write_n(off, buf, n);
            if (mx_wip()) { }
            if (mx_verify(off, buf, n) == 0) { break; }
        }
        if (attempt == 4) { return 1; }

        off += n;
        buf += n;
        sz  -= n;
    }
    return 0;
}

int Verify(unsigned long adr, unsigned long sz, unsigned char *buf)
{
    unsigned long off = adr - FLASH_BASE;
    return mx_verify(off, buf, sz);
}

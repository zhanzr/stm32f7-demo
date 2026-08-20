/**
  * @file    syscalls.c
  * @brief   Minimal newlib retarget layer for a bare-metal STM32F769 build.
  *
  * _write() is redirected to the USART1 console (PA9/PA10 via board/) so
  * printf() output is visible on the ST-Link VCOM at 115200 baud.
  */

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include "uart_printf.h"

/* Linker-provided end of static data / start of the SDRAM heap. */
extern char end[];

/* The SDRAM heap tops out at the end of the physical on-board SDRAM
 * (16 MB @ 0xC0000000 -> 0xC1000000; the FMC address window is larger but
 * only the first 16 MB are populated). newlib also calls _sbrk() during
 * __libc_init_array (before main), when accesses to the external SDRAM fault
 * on this board - so those early allocations are served from a small DTCM
 * reserve (the `.dcm_heap` linker section) instead, and the SDRAM heap is only
 * used after Board_Init() has brought the SDRAM up. */
#define HEAP_LIMIT ((char *)0xC1000000UL)

static volatile uint8_t sdram_heap_ready;

void Board_SdramReady(void)
{
    sdram_heap_ready = 1;
}

#ifdef __cplusplus
extern "C" {
#endif

void _init(void)
{
}
void _fini(void)
{
}

void _exit(int status)
{
    (void)status;
    while (1)
    {
    }
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    errno = ENOSYS;
    return -1;
}

/* printf()/putchar() output -> USART1 (PA9 TX). */
int _write(int file, char *ptr, int len)
{
    int i;
    (void)file;
    for (i = 0; i < len; i++)
    {
        UART_PutChar((unsigned char)ptr[i]);
    }
    return len;
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

void *_sbrk(int incr)
{
    extern char _dcm_heap_start[];
    extern char _dcm_heap_end[];
    static char *dcm_break = 0;
    static char *sdram_break = 0;
    char *p;

    if (!sdram_heap_ready)
    {
        /* Pre-main (newlib __libc_init_array) allocations: DTCM reserve. */
        if (dcm_break == 0)
        {
            dcm_break = _dcm_heap_start;
        }
        p = dcm_break;
        if (p + incr > _dcm_heap_end)
        {
            errno = ENOMEM;
            return (void *)-1;
        }
        dcm_break = p + incr;
        return p;
    }

    /* Normal operation: the SDRAM heap (grows up from `end`). */
    if (sdram_break == 0)
    {
        sdram_break = end;
    }
    p = sdram_break;
    if (p + incr > HEAP_LIMIT)
    {
        errno = ENOMEM;
        return (void *)-1;
    }
    sdram_break = p + incr;
    return p;
}

#ifdef __cplusplus
}
#endif

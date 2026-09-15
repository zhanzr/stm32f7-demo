/**
  * @file    syscalls.c
  * @brief   Minimal newlib retarget layer for a bare-metal STM32F746ZG build.
  *
  * _write() is redirected to the USART3 console (PD8/PD9 via board/) so
  * printf() output is visible on the ST-Link VCP at 115200 baud.
  *
  * The malloc() heap lives in the on-chip SRAM1 (always available, no init), so
  * _sbrk() just grows it up from `end`, capped at the end of SRAM (0x20040000).
  * There is no external SDRAM and no pre-main external-memory fault concern on
  * this board, unlike the disco's SDRAM design.
  */

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include "uart_printf.h"

/* Linker-provided end of static data / start of the heap. */
extern char end[];

/* Heap tops out at the end of the on-chip SRAM (0x20020000 SRAM1 112 KB + SRAM2
 * 16 KB contiguous -> 0x20040000). */
#define HEAP_LIMIT ((char *)0x20040000UL)

#ifdef __cplusplus
extern "C" {
#endif

/* ARM AEABI thread-pointer read. starm-clang's newlib implements errno as a
 * TLS variable (errno -> __tls), read through __aeabi_read_tp(). On an
 * unthreaded bare-metal target there is no thread pointer, so return a stable
 * static-address "thread" that __tls_offset/errno can index off of. GNU
 * newlib never references it (its errno is *__errno()), so GCC links find the
 * symbol unused and --gc-sections drops it. */
void *__aeabi_read_tp(void)
{
    static char tp_slot;
    return &tp_slot;
}

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

/* printf()/putchar() output -> USART3 (PD8 TX). */
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

/* Simple growing heap in SRAM1 (grows up from `end`, capped at 0x20040000). */
void *_sbrk(int incr)
{
    static char *break_ptr = 0;
    char *p;

    if (break_ptr == 0)
    {
        break_ptr = end;
    }
    p = break_ptr;
    if (p + incr > HEAP_LIMIT)
    {
        errno = ENOMEM;
        return (void *)-1;
    }
    break_ptr = p + incr;
    return p;
}

#ifdef __cplusplus
}
#endif
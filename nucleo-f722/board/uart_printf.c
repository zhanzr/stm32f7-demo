/**
  * @file    uart_printf.c
  * @brief   USART3 printf implementation for the NUCLEO-F722ZE board.
  *
  * USART3 is on PD8 (TX) / PD9 (RX), AF7, and is wired to the on-board
  * ST-Link VCP. Output is 115200 8-N-1, blocking (polled) so nothing is
  * dropped. USART3 clock source: PCLK1 (APB1, 54 MHz).
  */

#include "uart_printf.h"
#include "board.h"
#include "stm32f7xx_hal.h"

#include <stdio.h>
#include <stdarg.h>

static UART_HandleTypeDef huart3;

/* ------------------------------------------------------------------------ */
/* printf() replacement for armclang builds (see cmake/printf_rename.h).
 * armclang would otherwise turn printf into ARMCLIB's __2printf ABI, which
 * cannot be linked against newlib. vprintf() is not specialized by armclang,
 * so this thin wrapper keeps the standard printf() working over the UART. */
int bench_printf(const char *fmt, ...)
{
    va_list args;
    int r;

    va_start(args, fmt);
    r = vprintf(fmt, args);
    va_end(args);
    return r;
}

/* ------------------------------------------------------------------------ */
void UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* PD8 = USART3_TX, PD9 = USART3_RX (AF7). */
    GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 115200U;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);
}

/* ------------------------------------------------------------------------ */
int UART_PutChar(int ch)
{
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart3, &c, 1U, 1000U);
    return ch;
}

/* ------------------------------------------------------------------------ */
int UART_GetChar(void)
{
    uint8_t c;
    /* A streaming host can overrun the RX FIFO while the CPU is busy; the ORE/
     * FE flags then latch and every subsequent HAL_UART_Receive() fails. Clear
     * them on each poll so a latched error cannot wedge the RX path. */
    __HAL_UART_CLEAR_PEFLAG(&huart3);
    __HAL_UART_CLEAR_FEFLAG(&huart3);
    __HAL_UART_CLEAR_NEFLAG(&huart3);
    __HAL_UART_CLEAR_OREFLAG(&huart3);

    if (HAL_UART_Receive(&huart3, &c, 1U, 100U) == HAL_OK)
    {
        return c;
    }
    return -1;
}